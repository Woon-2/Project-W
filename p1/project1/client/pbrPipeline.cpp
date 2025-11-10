#include "pbrPipeline.hpp"
#include "shader.hpp"
#include "mesh.hpp"
#include "errorHandling.hpp"

namespace PBRPipeline {

// PBR Pipeline의 input layout을 위한 Vertex Buffer View 배열이
// mesh에 존재하지 않는다면, 추가한다.
// 0: position, 1: normal, 2: uv
void layoutMeshIfNeeded(const Mesh& mesh) {
	if (mesh.vbViewsByPipeline.contains("PBRPipeline")) {
		return;
	}

	auto [pvbViews, _] = mesh.vbViewsByPipeline.try_emplace("PBRPipeline");
	auto& vbViews = pvbViews->second;
	vbViews.reserve(3u);	// position, normal, uv

	DISPLAY_ERROR_STR( mesh.vbIdxMap.contains(mesh.name + "_VB_Position"),
		"[GFX Error] PBRPipeline::layoutMeshIfNeeded: " + mesh.name + "_VB_Position"
		"의 이름을 가진 정점 버퍼가 요구되었으나, 존재하지 않습니다.",
		false
	);
	DISPLAY_ERROR_STR( mesh.vbIdxMap.contains(mesh.name + "_VB_Normal"),
		"[GFX Error] PBRPipeline::layoutMeshIfNeeded: " + mesh.name + "_VB_Normal"
		"의 이름을 가진 정점 버퍼가 요구되었으나, 존재하지 않습니다.",
		false
	);
	DISPLAY_ERROR_STR( mesh.vbIdxMap.contains(mesh.name + "_VB_UV"),
		"[GFX Error] PBRPipeline::layoutMeshIfNeeded: " + mesh.name + "_VB_UV"
		"의 이름을 가진 정점 버퍼가 요구되었으나, 존재하지 않습니다.",
		false
	);

	auto& vbViewPos = mesh.vbViews[ mesh.vbIdxMap.at(mesh.name + "_VB_Position") ];
	auto& vbViewNormal = mesh.vbViews[ mesh.vbIdxMap.at(mesh.name + "_VB_Normal") ];
	auto& vbViewUV = mesh.vbViews[ mesh.vbIdxMap.at(mesh.name + "_VB_UV") ];

	vbViews.push_back(vbViewPos);
	vbViews.push_back(vbViewNormal);
	vbViews.push_back(vbViewUV);
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
	ThreadPool* threadPool, CommandListPool* commandListPool, std::vector<DrawEvent>&& drawEvents,
	std::vector<LightData>&& lightData, const CameraData& cameraData, const FrameData& frameData,
	std::size_t roomIdx
) : descriptorHeaps_(descriptorHeaps), pTexPool_(pTexPool), pTexArrayPool_(pTexArrayPool),
	pTexCubePool_(pTexCubePool), pSamPool_(pSamPool), pCmpSamPool_(pCmpSamPool),
	rootSig_(rootSig), shader_(shader), cmdQ_(cmdQ),
	viewport_(viewport), scissorRect_(scissorRect), rtv_(rtv), dsv_(dsv), pFence_(pFence),
	pResources_(pResources), threadPool_(threadPool), cmdListPool_(commandListPool), drawEvents_(std::move(drawEvents)),
	lightData_(std::move(lightData)), cameraData_(cameraData), frameData_(frameData), roomIdx_(roomIdx),
	rootParamIdxPDD_(rootSig->paramIdx("PerDrawcallData")),
	rootParamIdxPID_(rootSig->paramIdx("PerInstanceData")),
	rootParamIdxPFD_(rootSig->paramIdx("PerFrameData")),
	rootParamIdxLightData_(rootSig->paramIdx("LightData")),
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

	// 메시 데이터 업로드
	// 정렬을 통해 인스턴싱이 가능하도록 한다.
	std::sort(drawEvents_.begin(), drawEvents_.end());
	
	// perInstanceData를 static으로 선언하여
	// 매번 처음부터 메모리를 구축하지 않고 재사용할 수 있도록 한다.
	static auto perInstanceData = std::vector<PBRShader::PerInstanceData>();
	perInstanceData.resize(drawEvents_.size());

	const auto view = cameraData_.view;
	const auto viewProj = cameraData_.view * cameraData_.proj;

	// DrawEvents에 담겨있는 정보를 가공해 perInstanceData에 저장한다.
	std::ranges::transform(drawEvents_, perInstanceData.begin(),
		[view, viewProj](const PBRPipeline::DrawEvent& drawEvent) {
			return PBRShader::PerInstanceData{
				.wvp = mu::transpose(drawEvent.world * viewProj).getXmf(),
				.wv = mu::transpose(drawEvent.world * view).getXmf(),
				.wvNormal = mu::inverse(mu::Mat3x3(drawEvent.world * view)).getXmf()
			};
		}	
	);

	// perInstanceData의 내용을 바탕으로 GPU 데이터를 갱신한다.
	pResources_->perInstanceData.stage(roomIdx_, perInstanceData);
	perInstanceData.clear();

	// lightData를 static으로 선언하여
	// 매번 처음부터 메모리를 구축하지 않고 재사용할 수 있도록 한다.
	static auto lightData = std::vector<PBRShader::Light>();
	lightData.resize(lightData_.size());

	// LightData에 담겨있는 정보를 가공해 lightData에 저장한다.
	std::ranges::transform(lightData_, lightData.begin(),
		[view](const PBRPipeline::LightData& lightData) {
			return PBRShader::Light{
				.color = lightData.color.getXmf(),
				.falloff = lightData.falloff,
				.posV = mu::Vec3(mu::Vec4(lightData.pos, 1.f) * view).getXmf(),
				.cosTheta = lightData.cosTheta,
				.dirV = mu::NVec3(mu::Vec4(lightData.dir, 0.f) * view).getXmf(),
				.cosPhi = lightData.cosPhi,
				.atten = lightData.atten.getXmf(),
				.intensity = lightData.intensity,
				.type = etoi(lightData.type)
			};
		}	
	);

	// FrameData에 담겨있는 정보를 가공해 pfd에 저장한다.
	auto pfd = PBRShader::PerFrameData{
		.globalAmbient = frameData_.globalAmbient.getXmf(),
		.lightCnt = static_cast<u32t>(lightData.size())	// 여기서 lightData.size()를 호출하므로 
														// 이전에 lightData.clear()를 호출하면 안된다.
	};
	// pfd의 내용을 바탕으로 GPU 데이터를 갱신한다.
	pResources_->perFrameData.stage(roomIdx_, &pfd, 1u);

	// lightData의 내용을 바탕으로 GPU 데이터를 갱신한다.
	pResources_->lightData.stage(roomIdx_, lightData);
	lightData.clear();
}

// 셰이더에서 사용하는 GPU 데이터를 갱신한다.
// DrawEvents, CameraData, LightData, FrameData에 담겨있는 정보를 가공하여
// Resources 객체에 담긴, ShaderInputBuffer 인터페이스를 가지는 객체들에 옮겨담는다.
// 싱글스레드로 동작한다.
// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
void Dispatcher::updateGPUDataMultiThreaded() {
	if (drawEvents_.empty()) {
		return;
	}

	// 메시 데이터 업로드
	// 정렬을 통해 인스턴싱이 가능하도록 한다.
	std::sort(drawEvents_.begin(), drawEvents_.end());
	
	// perInstanceData를 static으로 선언하여
	// 매번 처음부터 메모리를 구축하지 않고 재사용할 수 있도록 한다.
	static auto perInstanceData = std::vector<PBRShader::PerInstanceData>();
	perInstanceData.resize(drawEvents_.size());

	// DrawEvent는 jobSize 단위로 스레드들에 분배될 것이므로,
	// 동기화를 위한 latch를 준비한다.
	// 이때, DrawEvent의 개수가 jobSize로 나누어 떨어지지 않을 경우를 대비한다.
	// perInstanceData의 양에 비해 lightData나 pfd의 양은 미미하다.
	// lightData와 pfd의 처리는 구태여 멀티스레드로 하지 않는다.
	auto latch = std::latch( drawEvents_.size() / jobSizeUpdate_
		+ ((drawEvents_.size() % jobSizeUpdate_) != 0)
	);

	const auto viewProj = cameraData_.view * cameraData_.proj;

	// drawEvents의 [accEventCnt, accEventCnt + jobSizeUpdate_) 범위의
	// 데이터를 가공해 perInstanceData의 대응되는 영역에 저장한다.
	std::size_t accEventCnt = 0u;
	while (accEventCnt + (jobSizeUpdate_ - 1) < drawEvents_.size()) {
		addJobUpdate( cameraData_.view, viewProj, drawEvents_.data() + accEventCnt,
			drawEvents_.data() + accEventCnt + jobSizeUpdate_,
			perInstanceData.data() + accEventCnt, latch
		);

		accEventCnt += jobSizeUpdate_;
	}
	
	// 찌꺼기 처리
	if (accEventCnt != drawEvents_.size()) {
		const auto lastJobSize = drawEvents_.size() - accEventCnt;

		addJobUpdate( cameraData_.view, viewProj, drawEvents_.data() + accEventCnt,
			drawEvents_.data() + accEventCnt + lastJobSize,
			perInstanceData.data() + accEventCnt, latch
		);
	}

	// lightData를 static으로 선언하여
	// 매번 처음부터 메모리를 구축하지 않고 재사용할 수 있도록 한다.
	static auto lightData = std::vector<PBRShader::Light>();
	lightData.resize(lightData_.size());

	// LightData에 담겨있는 정보를 가공해 lightData에 저장한다.
	std::ranges::transform(lightData_, lightData.begin(),
		[view = cameraData_.view](const PBRPipeline::LightData& lightData) {
			return PBRShader::Light{
				.color = lightData.color.getXmf(),
				.falloff = lightData.falloff,
				.posV = mu::Vec3(mu::Vec4(lightData.pos, 1.f) * view).getXmf(),
				.cosTheta = lightData.cosTheta,
				.dirV = mu::NVec3(mu::Vec4(lightData.dir, 0.f) * view).getXmf(),
				.cosPhi = lightData.cosPhi,
				.atten = lightData.atten.getXmf(),
				.intensity = lightData.intensity,
				.type = etoi(lightData.type)
			};
		}	
	);

	// FrameData에 담겨있는 정보를 가공해 pfd에 저장한다.
	auto pfd = PBRShader::PerFrameData{
		.globalAmbient = frameData_.globalAmbient.getXmf(),
		.lightCnt = static_cast<u32t>(lightData.size())	// 여기서 lightData.size()를 호출하므로 
														// 이전에 lightData.clear()를 호출하면 안된다.
	};
	// pfd의 내용을 바탕으로 GPU 데이터를 갱신한다.
	pResources_->perFrameData.stage(roomIdx_, &pfd, 1u);

	// lightData의 내용을 바탕으로 GPU 데이터를 갱신한다.
	pResources_->lightData.stage(roomIdx_, lightData);
	lightData.clear();

	// 동기화
	latch.wait();
	// 모든 데이터가 가공된 이후, GPU 데이터를 갱신한다.
	pResources_->perInstanceData.stage(roomIdx_, perInstanceData);
	perInstanceData.clear();
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

	// 바인드해야 하는 GPU 데이터는 다음 네 종류다. (셰이더 참고)
	// - PerInstanceData
	// - PerDrawcallData
	// - PerFrameData
	// - LightData

	// PerInstanceData 바인드
	pResources_->perInstanceData.bind(cmdList, rootParamIdxPID_, roomIdx_);
	// LightData 바인드
	pResources_->lightData.bind(cmdList, rootParamIdxLightData_, roomIdx_);
	// PerFrameData 바인드
	pResources_->perFrameData.bind(cmdList, rootParamIdxPFD_, roomIdx_);

	u32t idxDrawcall = 0u;

	// DrawEvent들을 하나씩 처리한다.
	for (const auto& drawEvent : drawEvents_) {
		// PerDrawcallData 바인드
		pResources_->perDrawcallData.cbuffers[idxDrawcall].bind(
			cmdList, rootParamIdxPDD_, roomIdx_
		);

		// PerDrawcallData GPU 데이터 갱신
		// (바인드와 GPU 데이터 갱신 순서는 상관없다.
		//  어차피 바인드는 GPU 명령이라 바로 실행되지 않기 때문에)
		auto perDrawcallData = PBRShader::PerDrawcallData{
			.material = PBRShader::Material{
				.idxAlbedo = drawEvent.subMesh->material.mapAlbedo.idxSrv,
				.idxMetallicSmoothness = drawEvent.subMesh->material.mapMetallicSmoothness.idxSrv,
				.idxNormal = drawEvent.subMesh->material.mapNormal.idxSrv,
				.idxEmmisive = drawEvent.subMesh->material.mapEmmisive.idxSrv,
				.idxAmbientOcllusion = drawEvent.subMesh->material.mapAmbientOcclusion.idxSrv,
				.cAlbedo = drawEvent.subMesh->material.constantAlbedo,
				.cRoughness = drawEvent.subMesh->material.constantRoughness,
				.cMetallic = drawEvent.subMesh->material.constantMetallic,
				.cAOStrength = drawEvent.subMesh->material.constantAOStrength,
				.cEmmisive = drawEvent.subMesh->material.constantEmmisive
			},
			.firstInstanceIdx = static_cast<u32t>(idxDrawcall)
		};
		pResources_->perDrawcallData.cbuffers[idxDrawcall].stage(
			roomIdx_, &perDrawcallData, 1u
		);

		layoutMeshIfNeeded(*drawEvent.mesh);
		auto& vbViews = drawEvent.mesh->vbViewsByPipeline.at("PBRPipeline");

		DISPLAY_ERROR_DX_VOID( cmdList->IASetVertexBuffers(
			0u, static_cast<UINT>(vbViews.size()), vbViews.data()
		), false );
		DISPLAY_ERROR_DX_VOID( cmdList->IASetIndexBuffer(&drawEvent.subMesh->ibView), false );

		DISPLAY_ERROR_DX_VOID( cmdList->DrawIndexedInstanced(
			static_cast<UINT>(drawEvent.subMesh->ibView.SizeInBytes / sizeof(u16t)),
			1u, 0u, 0, 0u
		), false );

		++idxDrawcall;
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

// DrawEvents의 정보들을 참고하여
// 드로우콜들을 수행한다.
// 멀티스레드로 동작한다.
void Dispatcher::drawMultiThreaded() {
	if (drawEvents_.empty()) {
		return;
	}

	// DrawEvent는 jobSize 단위로 스레드들에 분배될 것이므로,
	// 동기화를 위한 latch를 준비한다.
	// 이때, DrawEvent의 개수가 jobSize로 나누어 떨어지지 않을 경우를 대비한다.
	const std::size_t jobCnt = (drawEvents_.size() + (jobSizeDraw_ - 1)) / jobSizeDraw_;
	auto latch = std::latch(jobCnt);

	// 파악된 작업의 개수에 맞게 명령 컨텍스트들을 할당한다.
	std::list<CommandContext> cmdCtxs{};
	const auto requiredCmdListCnt = jobCnt;

	const auto allocatedCmdListCnt = cmdListPool_->alloc(
		requiredCmdListCnt, CommandListUsage::RenderingSlave, cmdCtxs
	);

	DISPLAY_ERROR_STR( allocatedCmdListCnt == requiredCmdListCnt,
		"[GFX Error] GFX::renderSampleShaderDispatch: 요청한 수 만큼의 명령 리스트를 할당받지 못했습니다.",
		false
	);
	if (allocatedCmdListCnt != requiredCmdListCnt) {
		// 필요한 만큼 명령 컨텍스트가 할당되지 않았을 경우,
		// 명령 컨텍스트들을 사용하지 않고 그대로 반납하며,
		// 함수도 그대로 반환한다.
		// 추후 이 경우에도 동작할 수 있도록 대응하도록 한다..
		cmdListPool_->free(CommandListUsage::RenderingSlave, std::move(cmdCtxs));
		return;
	}

	// drawEvents의 [accEventCnt, accEventCnt + jobSizeUpdate_) 범위의
	// 데이터를 가공해 perDrawcallData를 구축하고, 드로우콜을 수행한다.
	std::size_t accEventCnt = 0u;
	// 각 작업마다 명령 컨텍스트를 분배한다.
	auto currCmdCtx = cmdCtxs.begin();

	while (accEventCnt + (jobSizeDraw_ - 1) < drawEvents_.size()) {
		// 명령 컨텍스트 초기화
		auto hrCmdAllocReset = currCmdCtx->cmdAlloc->Reset();
		DISPLAY_ERROR_DX_HR( hrCmdAllocReset, false );
		if (hrCmdAllocReset < 0) {
			cmdListPool_->free(CommandListUsage::RenderingSlave, std::move(cmdCtxs));
			return;
		}
		auto hrCmdListReset = currCmdCtx->cmdList->Reset(currCmdCtx->cmdAlloc.Get(), nullptr);
		DISPLAY_ERROR_DX_HR( hrCmdListReset, false );
		if (hrCmdListReset < 0) {
			cmdListPool_->free(CommandListUsage::RenderingSlave, std::move(cmdCtxs));
			return;
		}

		// 명령 기록
		addJobDraw(currCmdCtx->cmdList.Get(), drawEvents_.data() + accEventCnt,
			drawEvents_.data() + accEventCnt + jobSizeDraw_, accEventCnt, latch
		);
		
		accEventCnt += jobSizeDraw_;
		++currCmdCtx;
	}

	// 찌꺼기 처리
	if (accEventCnt != drawEvents_.size()) {
		const auto lastJobSize = drawEvents_.size() - accEventCnt;
		// 명령 컨텍스트 초기화
		auto hrCmdAllocReset = currCmdCtx->cmdAlloc->Reset();
		DISPLAY_ERROR_DX_HR( hrCmdAllocReset, false );
		if (hrCmdAllocReset < 0) {
			cmdListPool_->free(CommandListUsage::RenderingSlave, std::move(cmdCtxs));
			return;
		}
		auto hrCmdListReset = currCmdCtx->cmdList->Reset(currCmdCtx->cmdAlloc.Get(), nullptr);
		DISPLAY_ERROR_DX_HR( hrCmdListReset, false );
		if (hrCmdListReset < 0) {
			cmdListPool_->free(CommandListUsage::RenderingSlave, std::move(cmdCtxs));
			return;
		}

		// 명령 기록
		addJobDraw(currCmdCtx->cmdList.Get(), drawEvents_.data() + accEventCnt,
			drawEvents_.data() + accEventCnt + lastJobSize , accEventCnt, latch
		);
	}

	// 동기화
	latch.wait();

	// 명령 기록 끝, 실행
	auto stagedCmdLists = std::vector<ID3D12CommandList*>(cmdCtxs.size(), nullptr);
	std::ranges::transform(cmdCtxs, stagedCmdLists.begin(),
		[](const CommandContext& cmdCtx) { return cmdCtx.cmdList.Get(); }	
	);

	DISPLAY_ERROR_DX_VOID( cmdQ_->ExecuteCommandLists(
		static_cast<UINT>(stagedCmdLists.size()), stagedCmdLists.data()
	), false );

	// Fence 객체에 사용한 명령 컨텍스트들을 연관시켜 놓는다.
	pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)]
		.splice( pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].end(), std::move(cmdCtxs) );
}

// 멀티스레드 작업 시, GPU 데이터 갱신 작업에 대해
// 단위 작업을 생성하여 스레드에 할당하는데 사용된다.
void MU_CALLCONV Dispatcher::addJobUpdate( mu::Mat4x4 view, const mu::Mat4x4& viewProj,
	const DrawEvent* pFirst, const DrawEvent* pLast, PBRShader::PerInstanceData* pOut,
	std::latch& latch
) {
	threadPool_->addJob( [=, &latch](){
		std::transform( pFirst, pLast, pOut,
			[view, viewProj](const PBRPipeline::DrawEvent& drawEvent) {
				return PBRShader::PerInstanceData{
					.wvp = mu::transpose(drawEvent.world * viewProj).getXmf(),
					.wv = mu::transpose(drawEvent.world * view).getXmf(),
					.wvNormal = mu::inverse(mu::Mat3x3(drawEvent.world * view)).getXmf()
				};
			}	
		);

		latch.count_down();
	});
}

// 멀티스레드 작업 시, 드로우콜들에 대해
// 단위 작업을 생성하여 스레드에 할당하는데 사용된다.
void Dispatcher::addJobDraw( ID3D12GraphicsCommandList* threadCmdList,
	const DrawEvent* pFirst, const DrawEvent* pLast,
	std::size_t firstInstanceIdx, std::latch& latch
) {
	const auto jobSize = pLast - pFirst;

	threadPool_->addJob([=, &latch]() {
		// 명령 컨텍스트마다 개별적으로 파이프라인 설정을 해주어야 한다.
		// (파이프라인 설정은 공유되지 않는다. 그렇더라.)
		DISPLAY_ERROR_DX_VOID(threadCmdList->SetGraphicsRootSignature(rootSig_->get()), false);
		DISPLAY_ERROR_DX_VOID(threadCmdList->SetPipelineState(shader_.Get()), false);
		DISPLAY_ERROR_DX_VOID(threadCmdList->OMSetRenderTargets(1u, &rtv_, false, &dsv_), false);
		DISPLAY_ERROR_DX_VOID(threadCmdList->RSSetViewports(1u, &viewport_), false);
		DISPLAY_ERROR_DX_VOID(threadCmdList->RSSetScissorRects(1u, &scissorRect_), false);

		// bindless 환경 세팅
		// d3d12단 Descriptor Heap, Descriptor Table 설정
		auto descriptorHeapsRaw = std::vector<ID3D12DescriptorHeap*>(descriptorHeaps_.size());
		std::ranges::transform(descriptorHeaps_, descriptorHeapsRaw.begin(),
			[](ComPtr<ID3D12DescriptorHeap>& comPtrHeap) { return comPtrHeap.Get(); }	
		);
		DISPLAY_ERROR_DX_VOID( threadCmdList->SetDescriptorHeaps(
			static_cast<UINT>(descriptorHeapsRaw.size()), descriptorHeapsRaw.data()
		), false );

		pTexPool_->bind(threadCmdList, rootParamIdxTexPool_);
		pTexArrayPool_->bind(threadCmdList, rootParamIdxTexArrayPool_);
		pTexCubePool_->bind(threadCmdList, rootParamIdxTexCubePool_);
		pSamPool_->bind(threadCmdList, rootParamIdxSamPool_);
		pCmpSamPool_->bind(threadCmdList, rootParamIdxCmpSamPool_);
				
		DISPLAY_ERROR_DX_VOID( threadCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST), false );

		// 바인드해야 하는 GPU 데이터는 다음 네 종류다. (셰이더 참고)
		// - PerInstanceData
		// - PerDrawcallData
		// - PerFrameData
		// - LightData

		// PerInstanceData 바인드
		pResources_->perInstanceData.bind(threadCmdList, rootParamIdxPID_, roomIdx_);
		// LightData 바인드
		pResources_->lightData.bind(threadCmdList, rootParamIdxLightData_, roomIdx_);
		// PerFrameData 바인드
		pResources_->perFrameData.bind(threadCmdList, rootParamIdxPFD_, roomIdx_);

		for ( auto idxDrawcall = firstInstanceIdx;
			idxDrawcall < firstInstanceIdx + jobSize;
			++idxDrawcall
		) {
			// DrawEvent의 정보를 기반으로 GPU 데이터 업데이트 및
			// 입력 조립기 설정을 하고 드로우콜을 수행한다.
			const auto& drawEvent = drawEvents_[idxDrawcall];

			// PerDrawcallData 바인드
			pResources_->perDrawcallData.cbuffers[idxDrawcall].bind(
				threadCmdList, rootParamIdxPDD_, roomIdx_
			);

			auto perDrawcallData = PBRShader::PerDrawcallData{
				.material = PBRShader::Material{
					.idxAlbedo = drawEvent.subMesh->material.mapAlbedo.idxSrv,
					.idxMetallicSmoothness = drawEvent.subMesh->material.mapMetallicSmoothness.idxSrv,
					.idxNormal = drawEvent.subMesh->material.mapNormal.idxSrv,
					.idxEmmisive = drawEvent.subMesh->material.mapEmmisive.idxSrv,
					.idxAmbientOcllusion = drawEvent.subMesh->material.mapAmbientOcclusion.idxSrv,
					.cAlbedo = drawEvent.subMesh->material.constantAlbedo,
					.cRoughness = drawEvent.subMesh->material.constantRoughness,
					.cMetallic = drawEvent.subMesh->material.constantMetallic,
					.cAOStrength = drawEvent.subMesh->material.constantAOStrength,
					.cEmmisive = drawEvent.subMesh->material.constantEmmisive
				},
				.firstInstanceIdx = static_cast<u32t>(idxDrawcall)
			};
			// PerDrawcallData GPU 데이터 갱신
			// (바인드와 GPU 데이터 갱신 순서는 상관없다.
			//  어차피 바인드는 GPU 명령이라 바로 실행되지 않기 때문에)
			pResources_->perDrawcallData.cbuffers[idxDrawcall].stage(
				roomIdx_, &perDrawcallData, 1u
			);

			layoutMeshIfNeeded(*drawEvent.mesh);
			auto& vbViews = drawEvent.mesh->vbViewsByPipeline.at("PBRPipeline");

			DISPLAY_ERROR_DX_VOID( threadCmdList->IASetVertexBuffers(
				0u, static_cast<UINT>(vbViews.size()), vbViews.data()
			), false );
			DISPLAY_ERROR_DX_VOID( threadCmdList->IASetIndexBuffer(&drawEvent.subMesh->ibView), false );

			DISPLAY_ERROR_DX_VOID( threadCmdList->DrawIndexedInstanced(
				static_cast<UINT>(drawEvent.subMesh->ibView.SizeInBytes / sizeof(u16t)),
				1u, 0u, 0, 0u
			), false );
		}

		// 명령 기록 종료
		DISPLAY_ERROR_DX_HR( threadCmdList->Close(), false );
		latch.count_down();
	} );
}

}	// namespace PBRPipeline