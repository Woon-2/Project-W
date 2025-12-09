#include "pch.hpp"
#include "skyboxPipeline.hpp"
#include "shader.hpp"
#include "mesh.hpp"
#include "errorHandling.hpp"

namespace SkyboxPipeline {

// Skybox Pipeline의 input layout을 위한 Vertex Buffer View 배열이
// mesh에 존재하지 않는다면, 추가한다.
// 0: position
void layoutMeshIfNeeded(const Mesh& mesh) {
	if (mesh.vbViewsByPipeline.contains("SkyboxPipeline")) {
		return;
	}

	auto [pvbViews, _] = mesh.vbViewsByPipeline.try_emplace("SkyboxPipeline");
	auto& vbViews = pvbViews->second;
	vbViews.reserve(1u);	// position

	DISPLAY_ERROR_STR( mesh.vbIdxMap.contains(mesh.name + "_VB_Position"),
		"[GFX Error] SkyboxPipeline::layoutMeshIfNeeded: " + mesh.name + "_VB_Position"
		"의 이름을 가진 정점 버퍼가 요구되었으나, 존재하지 않습니다.",
		false
	);

	auto& vbViewPos = mesh.vbViews[ mesh.vbIdxMap.at(mesh.name + "_VB_Position") ];

	vbViews.push_back(vbViewPos);
}

// GFX 객체로부터 필요한 인자들을 전달받자.
Dispatcher::Dispatcher(
	const std::vector<ComPtr<ID3D12DescriptorHeap>>& descriptorHeaps,
	DescriptorPool* pTexPool, DescriptorPool* pTexArrayPool,
	DescriptorPool* pTexCubePool, DescriptorPool* pSamPool,
	DescriptorPool* pCmpSamPool,
	const std::shared_ptr<RootSig>& rootSig, const ComPtr<ID3D12PipelineState>& shader,
	const ComPtr<ID3D12CommandQueue>& cmdQ, const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissorRect,
	D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv, Fence* pFence, Resources* pResources,
	CommandListPool* commandListPool, std::vector<DrawEvent>&& drawEvents,
	const CameraData& cameraData, std::size_t roomIdx
) : descriptorHeaps_(descriptorHeaps), pTexPool_(pTexPool), pTexArrayPool_(pTexArrayPool),
	pTexCubePool_(pTexCubePool), pSamPool_(pSamPool), pCmpSamPool_(pCmpSamPool),
	rootSig_(rootSig), shader_(shader), cmdQ_(cmdQ),
	viewport_(viewport), scissorRect_(scissorRect), rtv_(rtv), dsv_(dsv), pFence_(pFence),
	pResources_(pResources), cmdListPool_(commandListPool), drawEvents_(std::move(drawEvents)),
	cameraData_(cameraData), roomIdx_(roomIdx),
	rootParamIdxPDD_(rootSig->paramIdx("PerDrawcallData")),
	rootParamIdxPFD_(rootSig->paramIdx("PerFrameData")),
	rootParamIdxTexPool_(rootSig->paramIdx("TexturePool")),
	rootParamIdxTexArrayPool_(rootSig->paramIdx("TextureArrayPool")),
	rootParamIdxTexCubePool_(rootSig->paramIdx("TextureCubePool")),
	rootParamIdxSamPool_(rootSig->paramIdx("SamplerPool")),
	rootParamIdxCmpSamPool_(rootSig->paramIdx("ComparisonSamplerPool")) {}

// 셰이더에서 사용하는 GPU 데이터를 갱신한다.
// DrawEvents, CameraData, LightData, FrameData에 담겨있는 정보를 가공하여
// Resources 객체에 담긴, ShaderInputBuffer 인터페이스를 가지는 객체들에 옮겨담는다.
// 싱글스레드로 동작한다.
// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
void Dispatcher::updateGPUDataSingleThreaded() {
	if (drawEvents_.empty()) {
		return;
	}

	// CameraData에 담겨있는 정보를 가공해 pfd에 저장한다.
	auto pfd = SkyboxShader::PerFrameData{
		.viewProj = mu::transpose(cameraData_.view * cameraData_.proj).getXmf()
	};
	// pfd의 내용을 바탕으로 GPU 데이터를 갱신한다.
	pResources_->perFrameData.stage(roomIdx_, &pfd, 1u);
}

// DrawEvents의 정보들을 참고하여
// 드로우콜들을 수행한다.
// 싱글스레드로 동작한다.
void Dispatcher::drawSingleThreaded() {
	if (drawEvents_.empty()) {
		return;
	}

	// 명령 컨텍스트 할당
	CommandContext cmdCtx{};
	DISPLAY_ERROR_STR( cmdListPool_->allocOne(CommandListUsage::RenderingSlave, cmdCtx),
		"[GFX Error] GFX::drawSingleThreaded: 요청한 명령 리스트를 할당받지 못했습니다.", false
	);
	if (!cmdCtx.cmdList) {
		return;
	}

	// 명령 컨텍스트 초기화
	auto cmdList = cmdCtx.cmdList.Get();
	auto cmdAlloc = cmdCtx.cmdAlloc.Get();
	auto hrCmdAllocReset = cmdAlloc->Reset();
	DISPLAY_ERROR_DX_HR( hrCmdAllocReset, false );
	if (hrCmdAllocReset < 0) {
		cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx));
		return;
	}
	auto hrCmdListReset = cmdList->Reset(cmdAlloc, nullptr);
	DISPLAY_ERROR_DX_HR( hrCmdListReset, false );
	if (hrCmdListReset < 0) {
		cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx));
		return;
	}

	// 명령 기록 시작
	DISPLAY_ERROR_DX_VOID(cmdList->SetGraphicsRootSignature(rootSig_->get()), false);
	DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(shader_.Get()), false);
	DISPLAY_ERROR_DX_VOID(cmdList->OMSetRenderTargets(1u, &rtv_, false, &dsv_), false);
	DISPLAY_ERROR_DX_VOID(cmdList->RSSetViewports(1u, &viewport_), false);
	DISPLAY_ERROR_DX_VOID(cmdList->RSSetScissorRects(1u, &scissorRect_), false);

	// bindless 환경 세팅
	// d3d12단 Descriptor Heap, Descriptor Table 설정
	auto descriptorHeapsRaw = std::vector<ID3D12DescriptorHeap*>(descriptorHeaps_.size());
	std::ranges::transform(descriptorHeaps_, descriptorHeapsRaw.begin(),
		[](ComPtr<ID3D12DescriptorHeap>& comPtrHeap) { return comPtrHeap.Get(); }	
	);
	DISPLAY_ERROR_DX_VOID( cmdList->SetDescriptorHeaps(
		static_cast<UINT>(descriptorHeapsRaw.size()), descriptorHeapsRaw.data()
	), false );

	pTexPool_->bind(cmdList, rootParamIdxTexPool_);
	pTexArrayPool_->bind(cmdList, rootParamIdxTexArrayPool_);
	pTexCubePool_->bind(cmdList, rootParamIdxTexCubePool_);
	pSamPool_->bind(cmdList, rootParamIdxSamPool_);
	pCmpSamPool_->bind(cmdList, rootParamIdxCmpSamPool_);

	DISPLAY_ERROR_DX_VOID( cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST), false );

	// 바인드해야 하는 GPU 데이터는 다음 두 종류다. (셰이더 참고)
	// - PerDrawcallData
	// - PerFrameData

	// PerFrameData 바인드
	pResources_->perFrameData.bind(cmdList, rootParamIdxPFD_, roomIdx_);

	u32t idxDrawcall = 0u;

	for (auto& drawEvent : drawEvents_) {
		// PerDrawcallData 바인드
		pResources_->perDrawcallData.cbuffers[idxDrawcall].bind(
			cmdList, rootParamIdxPDD_, roomIdx_
		);

		// PerDrawcallData GPU 데이터 갱신
		// (바인드와 GPU 데이터 갱신 순서는 상관없다.
		//  어차피 바인드는 GPU 명령이라 바로 실행되지 않기 때문에)
		auto perDrawcallData = SkyboxShader::PerDrawcallData{
			.material = SkyboxShader::Material{
				.idxAlbedo = drawEvent.texSkybox->idxSrv
			}
		};
		pResources_->perDrawcallData.cbuffers[idxDrawcall].stage(
			roomIdx_, &perDrawcallData, 1u
		);

		layoutMeshIfNeeded(*drawEvent.mesh);
		auto& vbViews = drawEvent.mesh->vbViewsByPipeline.at("SkyboxPipeline");

		DISPLAY_ERROR_DX_VOID( cmdList->IASetVertexBuffers(
			0u, static_cast<UINT>(vbViews.size()), vbViews.data()
		), false );
		DISPLAY_ERROR_DX_VOID( cmdList->IASetIndexBuffer(&drawEvent.subMesh->ibView), false );

		DISPLAY_ERROR_DX_VOID( cmdList->DrawIndexedInstanced(
			static_cast<UINT>(drawEvent.subMesh->ibView.SizeInBytes / sizeof(u16t)),
			1u, 0u, 0, 0u
		), false );

	}

	auto hrClose = cmdList->Close();
	DISPLAY_ERROR_DX_HR( hrClose, false );
	if (hrClose < 0) {
		cmdListPool_->freeOne(CommandListUsage::RenderingSlave, std::move(cmdCtx));
		return;
	}

	// 명령 기록 끝, 실행
	ID3D12CommandList* stagedCmdLists[] = {cmdList};

	DISPLAY_ERROR_DX_VOID(cmdQ_->ExecuteCommandLists(1u, stagedCmdLists), false);
	
	// Fence 객체에 사용한 명령 컨텍스트를 연관시켜 놓는다.
	pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)]
		.push_back(std::move(cmdCtx));
}

}	// namespace SkyboxPipeline