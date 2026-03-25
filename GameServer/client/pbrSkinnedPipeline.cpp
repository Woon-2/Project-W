#include "pch.hpp"
#include "pbrSkinnedPipeline.hpp"
#include "sharedResources.hpp"
#include "shader.hpp"
#include "mesh.hpp"
#include "errorHandling.hpp"

namespace PBRSkinnedPipeline {

// PBR-skinned Pipeline 그림자 패스의 input layout을 위한 Vertex Buffer View 배열이
// mesh에 존재하지 않는다면, 추가한다.
// 0: position
void __layoutMeshIfNeededShadowPass(const Mesh& mesh) {
	if (mesh.vbViewsByPipeline.contains("PBRSkinnedPipeline_Shadow")) {
		return;
	}

	auto [pvbViews, _] = mesh.vbViewsByPipeline.try_emplace("PBRSkinnedPipeline_Shadow");
	auto& vbViews = pvbViews->second;
	vbViews.reserve(3u);	// position, boneIndices, boneWeights

	DISPLAY_ERROR_STR( mesh.vbIdxMap.contains(mesh.name + "_VB_Position"),
		"[GFX Error] PBRSkinnedPipeline::__layoutMeshIfNeededShadowPass: " + mesh.name + "_VB_Position"
		"의 이름을 가진 정점 버퍼가 요구되었으나, 존재하지 않습니다.",
		false
	);
	DISPLAY_ERROR_STR( mesh.vbIdxMap.contains(mesh.name + "_VB_BoneIndices"),
		"[GFX Error] PBRSkinnedPipeline::__layoutMeshIfNeededShadowPass: " + mesh.name + "_VB_BoneIndices"
		"의 이름을 가진 정점 버퍼가 요구되었으나, 존재하지 않습니다.",
		false
	);
	DISPLAY_ERROR_STR( mesh.vbIdxMap.contains(mesh.name + "_VB_BoneWeights"),
		"[GFX Error] PBRSkinnedPipeline::__layoutMeshIfNeededShadowPass: " + mesh.name + "_VB_BoneWeights"
		"의 이름을 가진 정점 버퍼가 요구되었으나, 존재하지 않습니다.",
		false
	);

	auto& vbViewPos = mesh.vbViews[ mesh.vbIdxMap.at(mesh.name + "_VB_Position") ];
	auto& vbViewBoneIndices = mesh.vbViews[ mesh.vbIdxMap.at(mesh.name + "_VB_BoneIndices") ];
	auto& vbViewBoneWeights = mesh.vbViews[ mesh.vbIdxMap.at(mesh.name + "_VB_BoneWeights") ];

	vbViews.push_back(vbViewPos);
	vbViews.push_back(vbViewBoneIndices);
	vbViews.push_back(vbViewBoneWeights);
}

// PBR-skinned Pipeline의 메인 렌더 패스의 input layout을 위한 Vertex Buffer View 배열이
// mesh에 존재하지 않는다면, 추가한다.
// 0: position, 1: normal, 2: tangent, 3: bitangent, 4: uv, 5: boneIndices, 6: boneWeights
void __layoutMeshIfNeededMainPass(const Mesh& mesh) {
	if (mesh.vbViewsByPipeline.contains("PBRSkinnedPipeline_Main")) {
		return;
	}

	auto [pvbViews, _] = mesh.vbViewsByPipeline.try_emplace("PBRSkinnedPipeline_Main");
	auto& vbViews = pvbViews->second;
	vbViews.reserve(7u);	// position, normal, tangent, bitangent, uv, boneIndices, boneWeights

	DISPLAY_ERROR_STR( mesh.vbIdxMap.contains(mesh.name + "_VB_Position"),
		"[GFX Error] PBRSkinnedPipeline::__layoutMeshIfNeededMainPass: " + mesh.name + "_VB_Position"
		"의 이름을 가진 정점 버퍼가 요구되었으나, 존재하지 않습니다.",
		false
	);
	DISPLAY_ERROR_STR( mesh.vbIdxMap.contains(mesh.name + "_VB_Normal"),
		"[GFX Error] PBRSkinnedPipeline::__layoutMeshIfNeededMainPass: " + mesh.name + "_VB_Normal"
		"의 이름을 가진 정점 버퍼가 요구되었으나, 존재하지 않습니다.",
		false
	);
	DISPLAY_ERROR_STR( mesh.vbIdxMap.contains(mesh.name + "_VB_Tangent"),
		"[GFX Error] PBRSkinnedPipeline::__layoutMeshIfNeededMainPass: " + mesh.name + "_VB_Tangent"
		"의 이름을 가진 정점 버퍼가 요구되었으나, 존재하지 않습니다.",
		false
	);
	DISPLAY_ERROR_STR( mesh.vbIdxMap.contains(mesh.name + "_VB_Bitangent"),
		"[GFX Error] PBRSkinnedPipeline::__layoutMeshIfNeededMainPass: " + mesh.name + "_VB_Bitangent"
		"의 이름을 가진 정점 버퍼가 요구되었으나, 존재하지 않습니다.",
		false
	);
	DISPLAY_ERROR_STR( mesh.vbIdxMap.contains(mesh.name + "_VB_UV"),
		"[GFX Error] PBRSkinnedPipeline::__layoutMeshIfNeededMainPass: " + mesh.name + "_VB_UV"
		"의 이름을 가진 정점 버퍼가 요구되었으나, 존재하지 않습니다.",
		false
	);
	DISPLAY_ERROR_STR( mesh.vbIdxMap.contains(mesh.name + "_VB_BoneIndices"),
		"[GFX Error] PBRSkinnedPipeline::__layoutMeshIfNeededMainPass: " + mesh.name + "_VB_BoneIndices"
		"의 이름을 가진 정점 버퍼가 요구되었으나, 존재하지 않습니다.",
		false
	);
	DISPLAY_ERROR_STR( mesh.vbIdxMap.contains(mesh.name + "_VB_BoneWeights"),
		"[GFX Error] PBRSkinnedPipeline::__layoutMeshIfNeededMainPass: " + mesh.name + "_VB_BoneWeights"
		"의 이름을 가진 정점 버퍼가 요구되었으나, 존재하지 않습니다.",
		false
	);

	auto& vbViewPos = mesh.vbViews[ mesh.vbIdxMap.at(mesh.name + "_VB_Position") ];
	auto& vbViewNormal = mesh.vbViews[ mesh.vbIdxMap.at(mesh.name + "_VB_Normal") ];
	auto& vbViewTangent = mesh.vbViews[ mesh.vbIdxMap.at(mesh.name + "_VB_Tangent") ];
	auto& vbViewBitangent = mesh.vbViews[ mesh.vbIdxMap.at(mesh.name + "_VB_Bitangent") ];
	auto& vbViewUV = mesh.vbViews[ mesh.vbIdxMap.at(mesh.name + "_VB_UV") ];
	auto& vbViewBoneIndices = mesh.vbViews[ mesh.vbIdxMap.at(mesh.name + "_VB_BoneIndices") ];
	auto& vbViewBoneWeights = mesh.vbViews[ mesh.vbIdxMap.at(mesh.name + "_VB_BoneWeights") ];

	vbViews.push_back(vbViewPos);
	vbViews.push_back(vbViewNormal);
	vbViews.push_back(vbViewTangent);
	vbViews.push_back(vbViewBitangent);
	vbViews.push_back(vbViewUV);
	vbViews.push_back(vbViewBoneIndices);
	vbViews.push_back(vbViewBoneWeights);
}

// PBR-skinned Pipeline의 input layout을 위한 Vertex Buffer View 배열이
// mesh에 존재하지 않는다면, 추가한다.
// 0: position, 1: normal, 2: tangent, 3: bitangent, 4: uv, 5: boneIndices, 6: boneWeights
void layoutMeshIfNeeded(const Mesh& mesh) {
	__layoutMeshIfNeededShadowPass(mesh);
	__layoutMeshIfNeededMainPass(mesh);
}

// GFX 객체로부터 필요한 인자들을 전달받자.
Dispatcher::Dispatcher(
	const std::vector<ComPtr<ID3D12DescriptorHeap>>& descriptorHeaps,
	DescriptorPool* pTexPool, DescriptorPool* pTexArrayPool,
	DescriptorPool* pTexCubePool, DescriptorPool* pSamPool,
	DescriptorPool* pCmpSamPool, DescriptorPool* pDsvPool,
	const std::shared_ptr<RootSig>& rootSig, const ComPtr<ID3D12PipelineState>& mainShader,
	const ComPtr<ID3D12PipelineState>& shadowShader,
	const ComPtr<ID3D12CommandQueue>& cmdQ, const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissorRect,
	D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv, Fence* pFence, Resources* pResources,
	ThreadPool* threadPool, CommandListPool* commandListPool, std::vector<DrawEvent>&& drawEvents,
	std::vector<LightData>&& lightData, const LightData& mainDirectionalLightData,
	const CameraData& cameraData, const FrameData& frameData,
	std::size_t roomIdx
) : descriptorHeaps_(descriptorHeaps), pTexPool_(pTexPool), pTexArrayPool_(pTexArrayPool),
	pTexCubePool_(pTexCubePool), pSamPool_(pSamPool), pCmpSamPool_(pCmpSamPool), pDsvPool_(pDsvPool),
	rootSig_(rootSig), mainShader_(mainShader), shadowShader_(shadowShader), cmdQ_(cmdQ),
	viewport_(viewport), scissorRect_(scissorRect), rtv_(rtv), dsv_(dsv), pFence_(pFence),
	pResources_(pResources), threadPool_(threadPool), cmdListPool_(commandListPool), drawEvents_(std::move(drawEvents)),
	lightData_(std::move(lightData)), mainDirectionalLightData_(mainDirectionalLightData),
	cameraData_(cameraData), frameData_(frameData), roomIdx_(roomIdx),
	rootParamIdxPDD_(rootSig->paramIdx("PerDrawcallData")),
	rootParamIdxPID_(rootSig->paramIdx("PerInstanceData")),
	rootParamIdxPFD_(rootSig->paramIdx("PerFrameData")),
	rootParamIdxLightData_(rootSig->paramIdx("LightData")),
	rootParamIdxBoneData_(rootSig->paramIdx("BoneData")),
	rootParamIdxTexPool_(rootSig->paramIdx("TexturePool")),
	rootParamIdxTexArrayPool_(rootSig->paramIdx("TextureArrayPool")),
	rootParamIdxTexCubePool_(rootSig->paramIdx("TextureCubePool")),
	rootParamIdxSamPool_(rootSig->paramIdx("SamplerPool")),
	rootParamIdxCmpSamPool_(rootSig->paramIdx("ComparisonSamplerPool")) {}

// draw event들을 H/W 인스턴싱을 위해 그룹화(정렬)한다.
// 가장 먼저 호출되어야 한다.
void Dispatcher::sortDrawEvents() {
	std::sort(drawEvents_.begin(), drawEvents_.end());
}

// shadow pass를 싱글 스레드로 수행한다.
// 1번째 렌더패스에 해당한다.
void Dispatcher::shadowPass() {
	shadowUpdate();
	shadowDraw();
}

// shadow pass를 멀티 스레드로 수행한다.
// 1번째 렌더패스에 해당한다.
void Dispatcher::shadowPassMT() {
	shadowUpdateMT();
	shadowDrawMT();
}

// main pass를 싱글 스레드로 수행한다.
// 2번째 렌더패스에 해당한다.
void Dispatcher::mainPass() {
	mainUpdate();
	mainDraw();
}

// main pass를 멀티 스레드로 수행한다.
// 2번째 렌더패스에 해당한다.
void Dispatcher::mainPassMT() {
	mainUpdateMT();
	mainDrawMT();
}

// shadow pass에서 사용하는 GPU 데이터를 갱신한다.
// DrawEvents, CameraData, LightData에 담겨있는 정보를 가공하여
// Resources 객체에 담긴, ShaderInputBuffer 인터페이스를 가지는 객체들에 옮겨담는다.
// 싱글스레드로 동작한다.
// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
void Dispatcher::shadowUpdate() {
	if (drawEvents_.empty()) {
		return;
	}

	// perInstanceData를 static으로 선언하여
	// 매번 처음부터 메모리를 구축하지 않고 재사용할 수 있도록 한다.
	static auto perInstanceData = std::vector<ShadowMapSkinnedShader::PerInstanceData>();
	perInstanceData.resize(drawEvents_.size());

	u32t boneUploadCnt = 0u;

	// DrawEvents에 담겨있는 정보를 가공해 perInstanceData에 저장한다.
	std::ranges::transform(drawEvents_, perInstanceData.begin(),
		[&boneUploadCnt](const PBRSkinnedPipeline::DrawEvent& drawEvent) {
			const auto ret = ShadowMapSkinnedShader::PerInstanceData{
				.world = mu::transpose(drawEvent.world).getXmf(),
				.rootBoneOffset = boneUploadCnt
			};
			boneUploadCnt += static_cast<u32t>( drawEvent.boneXforms.size() );
			return ret;
		}	
	);

	// perInstanceData의 내용을 바탕으로 GPU 데이터를 갱신한다.
	pResources_->shadowPass.perInstanceData.stage(roomIdx_, perInstanceData);
	perInstanceData.clear();

	// boneData를 static으로 선언하여
	// 매번 처음부터 메모리를 구축하지 않고 재사용할 수 있도록 한다.
	static auto boneData = std::vector<ShadowMapSkinnedShader::BoneData>();
	boneData.resize(boneUploadCnt);

	auto itBoneOut = boneData.begin();

	std::ranges::for_each( drawEvents_, [itBoneOut](const PBRSkinnedPipeline::DrawEvent& drawEvent) mutable {
		for (auto& boneXform : drawEvent.boneXforms) {
			*itBoneOut = ShadowMapSkinnedShader::BoneData{ mu::transpose(boneXform).getXmf() };
			++itBoneOut;
		}
	} );

	pResources_->shadowPass.boneData.stage(roomIdx_, boneData);
	boneData.clear();

	// main directional light의 내용을 가공해 pfd에 저장한다.
	ShadowMapSkinnedShader::PerFrameData pfd{};
	pfd.cascadeCount = mainDirectionalLightData_.cascadeCount;
	for (u32t i = 0u; i < mainDirectionalLightData_.cascadeCount; ++i) {
		pfd.lightVP[i] = mu::transpose(
			mainDirectionalLightData_.cascadeViews[i] * mainDirectionalLightData_.cascadeProjs[i]
		).getXmf();
	}
	// pfd의 내용을 바탕으로 GPU 데이터를 갱신한다.
	pResources_->shadowPass.perFrameData.stage(roomIdx_, &pfd, 1u);
}

// shadow pass에서 사용하는 GPU 데이터를 갱신한다.
// DrawEvents, CameraData, LightData에 담겨있는 정보를 가공하여
// Resources 객체에 담긴, ShaderInputBuffer 인터페이스를 가지는 객체들에 옮겨담는다.
// 멀티스레드로 동작한다.
// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
void Dispatcher::shadowUpdateMT() {
	if (drawEvents_.empty()) {
		return;
	}

	// perInstanceData를 static으로 선언하여
	// 매번 처음부터 메모리를 구축하지 않고 재사용할 수 있도록 한다.
	static auto perInstanceData = std::vector<ShadowMapSkinnedShader::PerInstanceData>();
	perInstanceData.resize(drawEvents_.size());

	// DrawEvent는 jobSize 단위로 스레드들에 분배될 것이므로,
	// 동기화를 위한 latch를 준비한다.
	// 이때, DrawEvent의 개수가 jobSize로 나누어 떨어지지 않을 경우를 대비한다.
	auto latch = std::latch( drawEvents_.size() / jobSizeUpdate_
		+ ((drawEvents_.size() % jobSizeUpdate_) != 0)
	);

	// drawEvents의 [accEventCnt, accEventCnt + jobSizeUpdate_) 범위의
	// 데이터를 가공해 perInstanceData의 대응되는 영역에 저장한다.
	std::size_t accEventCnt = 0u;
	while (accEventCnt + (jobSizeUpdate_ - 1) < drawEvents_.size()) {
		addJobShadowUpdate( drawEvents_.data() + accEventCnt,
			drawEvents_.data() + accEventCnt + jobSizeUpdate_,
			perInstanceData.data() + accEventCnt, latch
		);

		accEventCnt += jobSizeUpdate_;
	}
	
	// 찌꺼기 처리
	if (accEventCnt != drawEvents_.size()) {
		const auto lastJobSize = drawEvents_.size() - accEventCnt;

		addJobShadowUpdate( drawEvents_.data() + accEventCnt,
			drawEvents_.data() + accEventCnt + lastJobSize,
			perInstanceData.data() + accEventCnt, latch
		);
	}

	// main directional light의 내용을 가공해 pfd에 저장한다.
	ShadowMapSkinnedShader::PerFrameData pfd{};
	pfd.cascadeCount = mainDirectionalLightData_.cascadeCount;
	for (u32t i = 0u; i < mainDirectionalLightData_.cascadeCount; ++i) {
		pfd.lightVP[i] = mu::transpose(
			mainDirectionalLightData_.cascadeViews[i] * mainDirectionalLightData_.cascadeProjs[i]
		).getXmf();
	}
	// pfd의 내용을 바탕으로 GPU 데이터를 갱신한다.
	pResources_->shadowPass.perFrameData.stage(roomIdx_, &pfd, 1u);

	// 동기화
	latch.wait();

	// rootBoneOffset 계산은 앞에서부터 순서대로 이루어져야 하므로 병렬로 수행할 수 없다.
	// 따라서 동기화 후 rootBoneOffset 멤버를 갱신하도록 한다.
	for (std::size_t i = 1u; i < drawEvents_.size(); ++i) {
		perInstanceData[i].rootBoneOffset = static_cast<u32t>(
			drawEvents_[i-1].boneXforms.size() + perInstanceData[i-1].rootBoneOffset
		);
	}

	// boneData를 static으로 선언하여
	// 매번 처음부터 메모리를 구축하지 않고 재사용할 수 있도록 한다.
	static auto boneData = std::vector<ShadowMapSkinnedShader::BoneData>();
	boneData.resize(perInstanceData.back().rootBoneOffset + drawEvents_.back().boneXforms.size());

	auto itBoneOut = boneData.begin();

	std::ranges::for_each( drawEvents_, [itBoneOut](const PBRSkinnedPipeline::DrawEvent& drawEvent) mutable {
		for (auto& boneXform : drawEvent.boneXforms) {
			*itBoneOut = ShadowMapSkinnedShader::BoneData{ mu::transpose(boneXform).getXmf() };
			++itBoneOut;
		}
	} );

	pResources_->shadowPass.boneData.stage(roomIdx_, boneData);
	boneData.clear();

	// 모든 데이터가 가공된 이후, GPU 데이터를 갱신한다.
	pResources_->shadowPass.perInstanceData.stage(roomIdx_, perInstanceData);
	perInstanceData.clear();
}

// DrawEvents의 정보들을 참고하여
// shadow pass의 드로우콜들을 수행한다.
// 싱글스레드로 동작한다.
// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
void Dispatcher::shadowDraw() {
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

	// shadow map data를 미리 쿼리해놓는다.
	// (동일한 검색 연산의 반복을 피한다.)
	auto& shadowMapData = SharedResources::ShadowMap::shadowMapData.at("ShadowMap")[roomIdx_];

	// 명령 기록 시작
	DISPLAY_ERROR_DX_VOID(cmdList->SetGraphicsRootSignature(rootSig_->get()), false);
	DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(shadowShader_.Get()), false);

	// shadow pass에서는 자체적인 shadow map 텍스처의 dsv를 사용한다.
	auto shadowMapDsv = shadowMapData.dsv;
	DISPLAY_ERROR_DX_VOID( cmdList->OMSetRenderTargets(
		0u, nullptr, false, &shadowMapDsv
	), false );

	// shadow map을 렌더링할 때 쓰는 viewport와 scissor rectangle의 크기는
	// shadow map 텍스처의 크기와 같아야 한다.
	auto shadowViewport = D3D12_VIEWPORT{
		.TopLeftX = 0.f,
		.TopLeftY = 0.f,
		.Width = static_cast<float>(shadowMapData.width),
		.Height = static_cast<float>(shadowMapData.height),
		.MinDepth = 0.f,
		.MaxDepth = 1.f
	};
	auto shadowScissorRect = D3D12_RECT{
		.left = 0, .top = 0,
		.right = static_cast<LONG>(shadowMapData.width),
		.bottom = static_cast<LONG>(shadowMapData.height)
	};

	DISPLAY_ERROR_DX_VOID(cmdList->RSSetViewports(1u, &shadowViewport), false);
	DISPLAY_ERROR_DX_VOID(cmdList->RSSetScissorRects(1u, &shadowScissorRect), false);

	// bindless 환경 세팅
	// d3d12단 Descriptor Heap 설정
	auto descriptorHeapsRaw = std::vector<ID3D12DescriptorHeap*>(descriptorHeaps_.size());
	std::ranges::transform(descriptorHeaps_, descriptorHeapsRaw.begin(),
		[](ComPtr<ID3D12DescriptorHeap>& comPtrHeap) { return comPtrHeap.Get(); }	
	);
	DISPLAY_ERROR_DX_VOID( cmdList->SetDescriptorHeaps(
		static_cast<UINT>(descriptorHeapsRaw.size()), descriptorHeapsRaw.data()
	), false );

	// 별도의 텍스처 샘플링을 하지 않으므로 DescriptorPool 바인딩은 생략한다.

	DISPLAY_ERROR_DX_VOID( cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST), false );

	// 바인드해야 하는 GPU 데이터는 다음 네 종류다. (셰이더 참고)
	// - PerInstanceData
	// - PerDrawcallData
	// - PerFrameData
	// - BoneData

	// PerInstanceData 바인드
	pResources_->shadowPass.perInstanceData.bind(cmdList, rootParamIdxPID_, roomIdx_);
	// PerFrameData 바인드
	pResources_->shadowPass.perFrameData.bind(cmdList, rootParamIdxPFD_, roomIdx_);
	// BoneData 바인드
	pResources_->shadowPass.boneData.bind(cmdList, rootParamIdxBoneData_, roomIdx_);

	u32t idxDrawcall = 0u;

	// 인스턴싱을 적용한다.
	// equivalent하게 평가되는 DrawEvent들을 (같은 메시와 서브메시 사용)
	// 묶어서 instancing group으로 삼아 하나의 드로우콜로 처리한다.
	// 
	// [groupFirst, groupLast)는 하나의 instancing group을 표현한다.
	auto groupFirst = drawEvents_.begin();
	while (groupFirst != drawEvents_.end()) {
		auto& drawEvent = *groupFirst;

		auto groupLast = std::upper_bound(groupFirst, drawEvents_.end(), drawEvent);

		// PerDrawcallData 바인드
		pResources_->shadowPass.perDrawcallData.cbuffers[idxDrawcall].bind(
			cmdList, rootParamIdxPDD_, roomIdx_
		);

		// PerDrawcallData GPU 데이터 갱신
		// (바인드와 GPU 데이터 갱신 순서는 상관없다.
		//  어차피 바인드는 GPU 명령이라 바로 실행되지 않기 때문에)
		auto perDrawcallData = ShadowMapSkinnedShader::PerDrawcallData{
			// perInstanceData에서 현재 instancing group의 첫 번째 인스턴스의 인덱스
			.firstInstanceOffset = static_cast<u32t>(groupFirst - drawEvents_.begin())
		};
		pResources_->shadowPass.perDrawcallData.cbuffers[idxDrawcall].stage(
			roomIdx_, &perDrawcallData, 1u
		);

		layoutMeshIfNeeded(*drawEvent.mesh);
		auto& vbViews = drawEvent.mesh->vbViewsByPipeline.at("PBRSkinnedPipeline_Shadow");

		DISPLAY_ERROR_DX_VOID( cmdList->IASetVertexBuffers(
			0u, static_cast<UINT>(vbViews.size()), vbViews.data()
		), false );
		DISPLAY_ERROR_DX_VOID( cmdList->IASetIndexBuffer(&drawEvent.subMesh->ibView), false );

		const auto indexStride = drawEvent.subMesh->ibView.Format == DXGI_FORMAT_R16_UINT
			? sizeof(u16t) : sizeof(u32t);

		DISPLAY_ERROR_DX_VOID( cmdList->DrawIndexedInstanced(
			static_cast<UINT>(drawEvent.subMesh->ibView.SizeInBytes / indexStride),
			static_cast<UINT>(groupLast - groupFirst), 0u, 0, 0u
		), false );

		++idxDrawcall;

		groupFirst = groupLast;
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
// shadow pass의 드로우콜들을 수행한다.
// 멀티스레드로 동작한다.
// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
void Dispatcher::shadowDrawMT() {
	if (drawEvents_.empty()) {
		return;
	}

	// 인스턴싱을 적용한다.
	// equivalent하게 평가되는 DrawEvent들을 (같은 메시와 서브메시 사용)
	// 묶어서 instancing group으로 삼아 하나의 드로우콜로 처리한다.
	// 
	// instancingGroups는 drawEvents_에서 각 instancing group의 첫 원소를 가리키는 iterator들을 저장한다.
	// 그리고 sentinel 값으로 drawEvents_.end()를 저장한다.
	// * 때문에 instancing group의 총 개수는 instancingGroups.size()가 아닌 instancingGroups.size() - 1이다.
	// 
	// instancingGroups[k]와 instancingGroups[k+1]은
	// k번째 instancingGroup의 begin, end가 된다.
	// 이를 위해 sentinel 값을 추가하였다.
	static auto instancingGroups = std::vector<decltype(drawEvents_)::const_iterator>();
	
	auto itFirst = drawEvents_.cbegin();
	while (itFirst != drawEvents_.cend()) {
		auto itLast = std::upper_bound(itFirst, drawEvents_.cend(), *itFirst);
		instancingGroups.push_back(itFirst);
		itFirst = itLast;
	}
	instancingGroups.push_back(drawEvents_.cend());

	// instancing group들은 jobSize 단위로 스레드들에 분배될 것이므로,
	// 동기화를 위한 latch를 준비한다.
	// 이때, DrawEvent의 개수가 jobSize로 나누어 떨어지지 않을 경우를 대비한다.
	const std::size_t jobCnt = ( (instancingGroups.size() - 1u) + (jobSizeDraw_ - 1u)) / jobSizeDraw_;
	auto latch = std::latch(jobCnt);

	// 파악된 작업의 개수에 맞게 명령 컨텍스트들을 할당한다.
	std::list<CommandContext> cmdCtxs{};
	const auto requiredCmdListCnt = jobCnt;

	const auto allocatedCmdListCnt = cmdListPool_->alloc(
		requiredCmdListCnt, CommandListUsage::RenderingSlave, cmdCtxs
	);

	DISPLAY_ERROR_STR( allocatedCmdListCnt == requiredCmdListCnt,
		"[GFX Error] PBRSkinnedPipeline::Dispatcher::shadowDrawMT: 요청한 수 만큼의 명령 리스트를 할당받지 못했습니다.",
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

	
	// 각 스레드는 instancingGroups 내 jobSizeDraw_ 개의 instancing group을 맡아
	// 드로우콜 명령을 기록한다.
	// 
	// accDrawcallCnt 변수를 이용하여 이 변수의 값을 jobSizeDraw_만큼 증가시켜가며
	// instancingGroups의 [accDrawcallCnt, accDrawcallCnt + jobSizeDraw_) 범위의
	// 데이터를 가공해 각 instancing group의 드로우콜 명령을 기록하는 것으로 구현한다.
	//
	// jobSizeDraw_ 개로 나누어 떨어지지 않을 상황에 대응하기 위해
	// 별도의 찌꺼기 처리 코드를 둔다.
	std::size_t accDrawcallCnt = 0u;
	// 각 작업마다 명령 컨텍스트를 분배한다.
	auto currCmdCtx = cmdCtxs.begin();

	// shadow map data를 미리 쿼리해놓는다.
	// (동일한 검색 연산의 반복을 피한다.)
	auto& shadowMapData = SharedResources::ShadowMap::shadowMapData.at("ShadowMap")[roomIdx_];

	while (accDrawcallCnt + (jobSizeDraw_ - 1) < instancingGroups.size() - 1u) {
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
		addJobShadowDraw( currCmdCtx->cmdList.Get(), instancingGroups.data() + accDrawcallCnt,
			instancingGroups.data() + accDrawcallCnt + jobSizeDraw_, accDrawcallCnt, shadowMapData, latch
		);

		accDrawcallCnt += jobSizeDraw_;
		++currCmdCtx;
	}

	// 찌꺼기 처리
	if (accDrawcallCnt != instancingGroups.size() - 1u) {
		const auto lastJobSize = instancingGroups.size() - 1u - accDrawcallCnt;
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
		addJobShadowDraw( currCmdCtx->cmdList.Get(), instancingGroups.data() + accDrawcallCnt,
			instancingGroups.data() + accDrawcallCnt + lastJobSize, accDrawcallCnt, shadowMapData, latch
		);
	}

	// 동기화
	latch.wait();

	instancingGroups.clear();

	// 명령 기록 끝, 실행
	auto stagedCmdLists = std::vector<ID3D12CommandList*>(cmdCtxs.size(), nullptr);
	std::ranges::transform(cmdCtxs.begin(), cmdCtxs.end(),
		stagedCmdLists.begin(),
		[](const CommandContext& cmdCtx) { return cmdCtx.cmdList.Get(); }	
	);

	DISPLAY_ERROR_DX_VOID( cmdQ_->ExecuteCommandLists(
		static_cast<UINT>(stagedCmdLists.size()), stagedCmdLists.data()
	), false );

	// Fence 객체에 사용한 명령 컨텍스트들을 연관시켜 놓는다.
	pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)]
		.splice( pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].end(), std::move(cmdCtxs) );
}

// main pass에서 사용하는 GPU 데이터를 갱신한다.
// DrawEvents, CameraData, LightData, FrameData에 담겨있는 정보를 가공하여
// Resources 객체에 담긴, ShaderInputBuffer 인터페이스를 가지는 객체들에 옮겨담는다.
// 싱글스레드로 동작한다.
// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
void Dispatcher::mainUpdate() {
	if (drawEvents_.empty()) {
		return;
	}

	// perInstanceData를 static으로 선언하여
	// 매번 처음부터 메모리를 구축하지 않고 재사용할 수 있도록 한다.
	static auto perInstanceData = std::vector<PBRSkinnedShader::PerInstanceData>();
	perInstanceData.resize(drawEvents_.size());

	const auto view = cameraData_.view;
	const auto viewProj = cameraData_.view * cameraData_.proj;

	u32t boneUploadCnt = 0u;

	// DrawEvents에 담겨있는 정보를 가공해 perInstanceData에 저장한다.
	std::ranges::transform(drawEvents_, perInstanceData.begin(),
		[view, viewProj, &boneUploadCnt](const PBRSkinnedPipeline::DrawEvent& drawEvent) {
			auto ret = PBRSkinnedShader::PerInstanceData{
				.world = mu::transpose(drawEvent.world).getXmf(),
				.wvp = mu::transpose(drawEvent.world * viewProj).getXmf(),
				.wv = mu::transpose(drawEvent.world * view).getXmf(),
				.wvNormal = mu::inverse(mu::Mat3x3(drawEvent.world * view)).getXmf(),
				.rootBoneOffset = boneUploadCnt
			};
			boneUploadCnt += static_cast<u32t>( drawEvent.boneXforms.size() );
			return ret;
		}	
	);

	// perInstanceData의 내용을 바탕으로 GPU 데이터를 갱신한다.
	pResources_->mainPass.perInstanceData.stage(roomIdx_, perInstanceData);
	perInstanceData.clear();

	// boneData를 static으로 선언하여
	// 매번 처음부터 메모리를 구축하지 않고 재사용할 수 있도록 한다.
	static auto boneData = std::vector<PBRSkinnedShader::BoneData>();
	boneData.resize(boneUploadCnt);

	auto itBoneOut = boneData.begin();

	std::ranges::for_each( drawEvents_, [itBoneOut](const PBRSkinnedPipeline::DrawEvent& drawEvent) mutable {
		for (auto& boneXform : drawEvent.boneXforms) {
			*itBoneOut = PBRSkinnedShader::BoneData{ mu::transpose(boneXform).getXmf() };
			++itBoneOut;
		}
	} );

	pResources_->mainPass.boneData.stage(roomIdx_, boneData);
	boneData.clear();

	// lightData를 static으로 선언하여
	// 매번 처음부터 메모리를 구축하지 않고 재사용할 수 있도록 한다.
	static auto lightData = std::vector<PBRSkinnedShader::Light>();
	lightData.resize(lightData_.size());

	// LightData에 담겨있는 정보를 가공해 lightData에 저장한다.
	std::ranges::transform(lightData_, lightData.begin(),
		[view](const PBRSkinnedPipeline::LightData& lightData) {
			return PBRSkinnedShader::Light{
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

	// shadow map data를 미리 쿼리해놓는다.
	// (동일한 검색 연산의 반복을 피한다.)
	auto& shadowMapData = SharedResources::ShadowMap::shadowMapData.at("ShadowMap")[roomIdx_];

	// FrameData와 main directional light의 내용을 가공해 pfd에 저장한다.
	PBRShader::PerFrameData pfd{};
	pfd.globalAmbient     = frameData_.globalAmbient.getXmf();
	pfd.lightCnt          = static_cast<u32t>(lightData.size());
	pfd.cascadeCount      = mainDirectionalLightData_.cascadeCount;
	pfd.idxShadowMap      = shadowMapData.tex.idxSrv;
	pfd.cascadeSplitsFarV = mainDirectionalLightData_.cascadeSplitsFarV;
	for (u32t i = 0u; i < mainDirectionalLightData_.cascadeCount; ++i) {
		pfd.lightVP[i] = mu::transpose(
			mainDirectionalLightData_.cascadeViews[i] * mainDirectionalLightData_.cascadeProjs[i]
		).getXmf();
	}
	// pfd의 내용을 바탕으로 GPU 데이터를 갱신한다.
	pResources_->mainPass.perFrameData.stage(roomIdx_, &pfd, 1u);

	// lightData의 내용을 바탕으로 GPU 데이터를 갱신한다.
	pResources_->mainPass.lightData.stage(roomIdx_, lightData);
	lightData.clear();
}

// main pass에서 사용하는 GPU 데이터를 갱신한다.
// DrawEvents, CameraData, LightData, FrameData에 담겨있는 정보를 가공하여
// Resources 객체에 담긴, ShaderInputBuffer 인터페이스를 가지는 객체들에 옮겨담는다.
// 멀티스레드로 동작한다.
// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
void Dispatcher::mainUpdateMT() {
	if (drawEvents_.empty()) {
		return;
	}

	// 메시 데이터 업로드
	
	// perInstanceData를 static으로 선언하여
	// 매번 처음부터 메모리를 구축하지 않고 재사용할 수 있도록 한다.
	static auto perInstanceData = std::vector<PBRSkinnedShader::PerInstanceData>();
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
		addJobMainUpdate( cameraData_.view, viewProj, drawEvents_.data() + accEventCnt,
			drawEvents_.data() + accEventCnt + jobSizeUpdate_,
			perInstanceData.data() + accEventCnt, latch
		);

		accEventCnt += jobSizeUpdate_;
	}
	
	// 찌꺼기 처리
	if (accEventCnt != drawEvents_.size()) {
		const auto lastJobSize = drawEvents_.size() - accEventCnt;

		addJobMainUpdate( cameraData_.view, viewProj, drawEvents_.data() + accEventCnt,
			drawEvents_.data() + accEventCnt + lastJobSize,
			perInstanceData.data() + accEventCnt, latch
		);
	}

	// lightData를 static으로 선언하여
	// 매번 처음부터 메모리를 구축하지 않고 재사용할 수 있도록 한다.
	static auto lightData = std::vector<PBRSkinnedShader::Light>();
	lightData.resize(lightData_.size());

	// LightData에 담겨있는 정보를 가공해 lightData에 저장한다.
	std::ranges::transform(lightData_, lightData.begin(),
		[view = cameraData_.view](const PBRSkinnedPipeline::LightData& lightData) {
			return PBRSkinnedShader::Light{
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

	// shadow map data를 미리 쿼리해놓는다.
	// (동일한 검색 연산의 반복을 피한다.)
	auto& shadowMapData = SharedResources::ShadowMap::shadowMapData.at("ShadowMap")[roomIdx_];

	// FrameData와 main directional light의 내용을 가공해 pfd에 저장한다.
	PBRShader::PerFrameData pfd{};
	pfd.globalAmbient     = frameData_.globalAmbient.getXmf();
	pfd.lightCnt          = static_cast<u32t>(lightData.size());
	pfd.cascadeCount      = mainDirectionalLightData_.cascadeCount;
	pfd.idxShadowMap      = shadowMapData.tex.idxSrv;
	pfd.cascadeSplitsFarV = mainDirectionalLightData_.cascadeSplitsFarV;
	for (u32t i = 0u; i < mainDirectionalLightData_.cascadeCount; ++i) {
		pfd.lightVP[i] = mu::transpose(
			mainDirectionalLightData_.cascadeViews[i] * mainDirectionalLightData_.cascadeProjs[i]
		).getXmf();
	}
	// pfd의 내용을 바탕으로 GPU 데이터를 갱신한다.
	pResources_->mainPass.perFrameData.stage(roomIdx_, &pfd, 1u);

	// lightData의 내용을 바탕으로 GPU 데이터를 갱신한다.
	pResources_->mainPass.lightData.stage(roomIdx_, lightData);
	lightData.clear();

	// 동기화
	latch.wait();

	// rootBoneOffset 계산은 앞에서부터 순서대로 이루어져야 하므로 병렬로 수행할 수 없다.
	// 따라서 동기화 후 rootBoneOffset 멤버를 갱신하도록 한다.
	for (std::size_t i = 1u; i < drawEvents_.size(); ++i) {
		perInstanceData[i].rootBoneOffset = static_cast<u32t>(
			drawEvents_[i-1].boneXforms.size() + perInstanceData[i-1].rootBoneOffset
		);
	}

	// boneData를 static으로 선언하여
	// 매번 처음부터 메모리를 구축하지 않고 재사용할 수 있도록 한다.
	static auto boneData = std::vector<PBRSkinnedShader::BoneData>();
	boneData.resize(perInstanceData.back().rootBoneOffset + drawEvents_.back().boneXforms.size());

	auto itBoneOut = boneData.begin();

	std::ranges::for_each( drawEvents_, [itBoneOut](const PBRSkinnedPipeline::DrawEvent& drawEvent) mutable {
		for (auto& boneXform : drawEvent.boneXforms) {
			*itBoneOut = PBRSkinnedShader::BoneData{ mu::transpose(boneXform).getXmf() };
			++itBoneOut;
		}
	} );

	pResources_->mainPass.boneData.stage(roomIdx_, boneData);
	boneData.clear();

	// 모든 데이터가 가공된 이후, GPU 데이터를 갱신한다.
	pResources_->mainPass.perInstanceData.stage(roomIdx_, perInstanceData);
	perInstanceData.clear();
}

// DrawEvents의 정보들을 참고하여
// main pass의 드로우콜들을 수행한다.
// 싱글스레드로 동작한다.
// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
void Dispatcher::mainDraw() {
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
	DISPLAY_ERROR_DX_VOID(cmdList->SetPipelineState(mainShader_.Get()), false);
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

	// 바인드해야 하는 GPU 데이터는 다음 다섯 종류다. (셰이더 참고)
	// - PerInstanceData
	// - PerDrawcallData
	// - PerFrameData
	// - LightData
	// - BoneData

	// PerInstanceData 바인드
	pResources_->mainPass.perInstanceData.bind(cmdList, rootParamIdxPID_, roomIdx_);
	// PerFrameData 바인드
	pResources_->mainPass.perFrameData.bind(cmdList, rootParamIdxPFD_, roomIdx_);
	// LightData 바인드
	pResources_->mainPass.lightData.bind(cmdList, rootParamIdxLightData_, roomIdx_);
	// BoneData 바인드
	pResources_->mainPass.boneData.bind(cmdList, rootParamIdxBoneData_, roomIdx_);

	u32t idxDrawcall = 0u;

	// 인스턴싱을 적용한다.
	// equivalent하게 평가되는 DrawEvent들을 (같은 메시와 서브메시 사용)
	// 묶어서 instancing group으로 삼아 하나의 드로우콜로 처리한다.
	// 
	// [groupFirst, groupLast)는 하나의 instancing group을 표현한다.
	auto groupFirst = drawEvents_.begin();
	while (groupFirst != drawEvents_.end()) {
		auto& drawEvent = *groupFirst;

		auto groupLast = std::upper_bound(groupFirst, drawEvents_.end(), drawEvent);

		// PerDrawcallData 바인드
		pResources_->mainPass.perDrawcallData.cbuffers[idxDrawcall].bind(
			cmdList, rootParamIdxPDD_, roomIdx_
		);

		// PerDrawcallData GPU 데이터 갱신
		// (바인드와 GPU 데이터 갱신 순서는 상관없다.
		//  어차피 바인드는 GPU 명령이라 바로 실행되지 않기 때문에)
		auto perDrawcallData = PBRSkinnedShader::PerDrawcallData{
			.material = PBRSkinnedShader::Material{
				.idxAlbedo = drawEvent.material->mapAlbedo.idxSrv,
				.idxMetallicSmoothness = drawEvent.material->mapMetallicSmoothness.idxSrv,
				.idxNormal = drawEvent.material->mapNormal.idxSrv,
				.idxEmmisive = drawEvent.material->mapEmmisive.idxSrv,
				.idxAmbientOcllusion = drawEvent.material->mapAmbientOcclusion.idxSrv,
				.cAlbedo = drawEvent.material->constantAlbedo,
				.cRoughness = drawEvent.material->constantRoughness,
				.cMetallic = drawEvent.material->constantMetallic,
				.cAOStrength = drawEvent.material->constantAOStrength,
				.cEmmisive = drawEvent.material->constantEmmisive
			},
			// perInstanceData에서 현재 instancing group의 첫 번째 인스턴스의 인덱스
			.firstInstanceOffset = static_cast<u32t>(groupFirst - drawEvents_.begin())
		};
		pResources_->mainPass.perDrawcallData.cbuffers[idxDrawcall].stage(
			roomIdx_, &perDrawcallData, 1u
		);

		layoutMeshIfNeeded(*drawEvent.mesh);
		auto& vbViews = drawEvent.mesh->vbViewsByPipeline.at("PBRSkinnedPipeline_Main");

		DISPLAY_ERROR_DX_VOID( cmdList->IASetVertexBuffers(
			0u, static_cast<UINT>(vbViews.size()), vbViews.data()
		), false );
		DISPLAY_ERROR_DX_VOID( cmdList->IASetIndexBuffer(&drawEvent.subMesh->ibView), false );

		const auto indexStride = drawEvent.subMesh->ibView.Format == DXGI_FORMAT_R16_UINT
			? sizeof(u16t) : sizeof(u32t);

		DISPLAY_ERROR_DX_VOID( cmdList->DrawIndexedInstanced(
			static_cast<UINT>(drawEvent.subMesh->ibView.SizeInBytes / indexStride),
			static_cast<UINT>(groupLast - groupFirst), 0u, 0, 0u
		), false );

		++idxDrawcall;

		groupFirst = groupLast;
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
// main pass의 드로우콜들을 수행한다.
// 멀티스레드로 동작한다.
// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
void Dispatcher::mainDrawMT() {
	if (drawEvents_.empty()) {
		return;
	}

	// 인스턴싱을 적용한다.
	// equivalent하게 평가되는 DrawEvent들을 (같은 메시와 서브메시 사용)
	// 묶어서 instancing group으로 삼아 하나의 드로우콜로 처리한다.
	// 
	// instancingGroups는 drawEvents_에서 각 instancing group의 첫 원소를 가리키는 iterator들을 저장한다.
	// 그리고 sentinel 값으로 drawEvents_.end()를 저장한다.
	// * 때문에 instancing group의 총 개수는 instancingGroups.size()가 아닌 instancingGroups.size() - 1이다.
	// 
	// instancingGroups[k]와 instancingGroups[k+1]은
	// k번째 instancingGroup의 begin, end가 된다.
	// 이를 위해 sentinel 값을 추가하였다.
	static auto instancingGroups = std::vector<decltype(drawEvents_)::const_iterator>();
	
	auto itFirst = drawEvents_.cbegin();
	while (itFirst != drawEvents_.cend()) {
		auto itLast = std::upper_bound(itFirst, drawEvents_.cend(), *itFirst);
		instancingGroups.push_back(itFirst);
		itFirst = itLast;
	}
	instancingGroups.push_back(drawEvents_.cend());

	// instancing group들은 jobSize 단위로 스레드들에 분배될 것이므로,
	// 동기화를 위한 latch를 준비한다.
	// 이때, DrawEvent의 개수가 jobSize로 나누어 떨어지지 않을 경우를 대비한다.
	const std::size_t jobCnt = ( (instancingGroups.size() - 1u) + (jobSizeDraw_ - 1u)) / jobSizeDraw_;
	auto latch = std::latch(jobCnt);

	// 파악된 작업의 개수에 맞게 명령 컨텍스트들을 할당한다.
	std::list<CommandContext> cmdCtxs{};
	const auto requiredCmdListCnt = jobCnt;

	const auto allocatedCmdListCnt = cmdListPool_->alloc(
		requiredCmdListCnt, CommandListUsage::RenderingSlave, cmdCtxs
	);

	DISPLAY_ERROR_STR( allocatedCmdListCnt == requiredCmdListCnt,
		"[GFX Error] PBRSkinnedPipeline::Dispatcher::mainDrawMT: 요청한 수 만큼의 명령 리스트를 할당받지 못했습니다.",
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

	
	// 각 스레드는 instancingGroups 내 jobSizeDraw_ 개의 instancing group을 맡아
	// 드로우콜 명령을 기록한다.
	// 
	// accDrawcallCnt 변수를 이용하여 이 변수의 값을 jobSizeDraw_만큼 증가시켜가며
	// instancingGroups의 [accDrawcallCnt, accDrawcallCnt + jobSizeDraw_) 범위의
	// 데이터를 가공해 각 instancing group의 드로우콜 명령을 기록하는 것으로 구현한다.
	//
	// jobSizeDraw_ 개로 나누어 떨어지지 않을 상황에 대응하기 위해
	// 별도의 찌꺼기 처리 코드를 둔다.
	std::size_t accDrawcallCnt = 0u;
	// 각 작업마다 명령 컨텍스트를 분배한다.
	auto currCmdCtx = cmdCtxs.begin();

	while (accDrawcallCnt + (jobSizeDraw_ - 1) < instancingGroups.size() - 1u) {
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
		addJobMainDraw( currCmdCtx->cmdList.Get(), instancingGroups.data() + accDrawcallCnt,
			instancingGroups.data() + accDrawcallCnt + jobSizeDraw_, accDrawcallCnt, latch
		);

		accDrawcallCnt += jobSizeDraw_;
		++currCmdCtx;
	}

	// 찌꺼기 처리
	if (accDrawcallCnt != instancingGroups.size() - 1u) {
		const auto lastJobSize = instancingGroups.size() - 1u - accDrawcallCnt;
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
		addJobMainDraw( currCmdCtx->cmdList.Get(), instancingGroups.data() + accDrawcallCnt,
			instancingGroups.data() + accDrawcallCnt + lastJobSize, accDrawcallCnt, latch
		);
	}

	// 동기화
	latch.wait();

	instancingGroups.clear();

	// 명령 기록 끝, 실행
	auto stagedCmdLists = std::vector<ID3D12CommandList*>(cmdCtxs.size(), nullptr);
	std::ranges::transform(cmdCtxs.begin(), cmdCtxs.end(),
		stagedCmdLists.begin(),
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
void MU_CALLCONV Dispatcher::addJobMainUpdate( mu::Mat4x4 view, const mu::Mat4x4& viewProj,
	const DrawEvent* pFirst, const DrawEvent* pLast, PBRSkinnedShader::PerInstanceData* pOut,
	std::latch& latch
) {
	threadPool_->addJob( [=, &latch](){
		std::transform( pFirst, pLast, pOut,
			[view, viewProj](const PBRSkinnedPipeline::DrawEvent& drawEvent) {
				return PBRSkinnedShader::PerInstanceData{
					.world = mu::transpose(drawEvent.world).getXmf(),
					.wvp = mu::transpose(drawEvent.world * viewProj).getXmf(),
					.wv = mu::transpose(drawEvent.world * view).getXmf(),
					.wvNormal = mu::inverse(mu::Mat3x3(drawEvent.world * view)).getXmf()
					// rootBoneOffset 계산은 앞에서부터 순서대로 이루어져야 하므로 병렬로 수행할 수 없다.
					// 따라서 동기화 후 rootBoneOffset 멤버를 갱신하도록 한다.
				};
			}	
		);

		latch.count_down();
	});
}

// 멀티스레드 작업 시, 드로우콜들에 대해
// 단위 작업을 생성하여 스레드에 할당하는데 사용된다.
void Dispatcher::addJobMainDraw( ID3D12GraphicsCommandList* threadCmdList,
	const std::vector<DrawEvent>::const_iterator* pItFirst,
	const std::vector<DrawEvent>::const_iterator* pItLast,
	std::size_t firstDrawcallIdx, std::latch& latch
) {
	threadPool_->addJob([=, &latch]() {
		// 명령 컨텍스트마다 개별적으로 파이프라인 설정을 해주어야 한다.
		// (파이프라인 설정은 공유되지 않는다. 그렇더라.)
		DISPLAY_ERROR_DX_VOID(threadCmdList->SetGraphicsRootSignature(rootSig_->get()), false);
		DISPLAY_ERROR_DX_VOID(threadCmdList->SetPipelineState(mainShader_.Get()), false);
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

		// 바인드해야 하는 GPU 데이터는 다음 다섯 종류다. (셰이더 참고)
		// - PerInstanceData
		// - PerDrawcallData
		// - PerFrameData
		// - LightData
		// - BoneData

		// PerInstanceData 바인드
		pResources_->mainPass.perInstanceData.bind(threadCmdList, rootParamIdxPID_, roomIdx_);
		// PerFrameData 바인드
		pResources_->mainPass.perFrameData.bind(threadCmdList, rootParamIdxPFD_, roomIdx_);
		// LightData 바인드
		pResources_->mainPass.lightData.bind(threadCmdList, rootParamIdxLightData_, roomIdx_);
		// BoneData 바인드
		pResources_->mainPass.boneData.bind(threadCmdList, rootParamIdxBoneData_, roomIdx_);

		std::size_t idxDrawcall = firstDrawcallIdx;

		// [pItFirst, pItLast]는 각 instancing group의 시작점(혹은 sentinel)이 되는
		// iterator들을 표현한다.
		auto pGroup = pItFirst;

		while (pGroup != pItLast) {
			// [groupFirst, groupLast)는 하나의 instancing group을 표현한다.
			auto groupFirst = *pGroup;
			auto groupLast = *(pGroup + 1);

			const auto& drawEvent = *groupFirst;

			// PerDrawcallData 바인드
			pResources_->mainPass.perDrawcallData.cbuffers[idxDrawcall].bind(
				threadCmdList, rootParamIdxPDD_, roomIdx_
			);

			auto perDrawcallData = PBRSkinnedShader::PerDrawcallData{
				.material = PBRSkinnedShader::Material{
					.idxAlbedo = drawEvent.material->mapAlbedo.idxSrv,
					.idxMetallicSmoothness = drawEvent.material->mapMetallicSmoothness.idxSrv,
					.idxNormal = drawEvent.material->mapNormal.idxSrv,
					.idxEmmisive = drawEvent.material->mapEmmisive.idxSrv,
					.idxAmbientOcllusion = drawEvent.material->mapAmbientOcclusion.idxSrv,
					.cAlbedo = drawEvent.material->constantAlbedo,
					.cRoughness = drawEvent.material->constantRoughness,
					.cMetallic = drawEvent.material->constantMetallic,
					.cAOStrength = drawEvent.material->constantAOStrength,
					.cEmmisive = drawEvent.material->constantEmmisive
				},
				// perInstanceData에서 현재 instancing group의 첫 번째 인스턴스의 인덱스
				.firstInstanceOffset = static_cast<u32t>(groupFirst - drawEvents_.begin())
			};
			// PerDrawcallData GPU 데이터 갱신
			// (바인드와 GPU 데이터 갱신 순서는 상관없다.
			//  어차피 바인드는 GPU 명령이라 바로 실행되지 않기 때문에)
			pResources_->mainPass.perDrawcallData.cbuffers[idxDrawcall].stage(
				roomIdx_, &perDrawcallData, 1u
			);

			layoutMeshIfNeeded(*drawEvent.mesh);
			auto& vbViews = drawEvent.mesh->vbViewsByPipeline.at("PBRSkinnedPipeline_Main");

			DISPLAY_ERROR_DX_VOID( threadCmdList->IASetVertexBuffers(
				0u, static_cast<UINT>(vbViews.size()), vbViews.data()
			), false );
			DISPLAY_ERROR_DX_VOID( threadCmdList->IASetIndexBuffer(&drawEvent.subMesh->ibView), false );

			const auto indexStride = drawEvent.subMesh->ibView.Format == DXGI_FORMAT_R16_UINT
				? sizeof(u16t) : sizeof(u32t);

			DISPLAY_ERROR_DX_VOID( threadCmdList->DrawIndexedInstanced(
				static_cast<UINT>(drawEvent.subMesh->ibView.SizeInBytes / indexStride),
				static_cast<UINT>(groupLast - groupFirst), 0u, 0, 0u
			), false );

			++idxDrawcall;
			++pGroup;
		}

		// 명령 기록 종료
		DISPLAY_ERROR_DX_HR( threadCmdList->Close(), false );
		latch.count_down();
	} );
}

// 멀티스레드 작업 시, GPU 데이터 갱신 작업에 대해
// 단위 작업을 생성하여 스레드에 할당하는데 사용된다.
void Dispatcher::addJobShadowUpdate( const DrawEvent* pFirst,
	const DrawEvent* pLast, ShadowMapSkinnedShader::PerInstanceData* pOut,
	std::latch& latch
) {
	threadPool_->addJob( [=, &latch](){
		std::transform( pFirst, pLast, pOut,
			[](const PBRSkinnedPipeline::DrawEvent& drawEvent) {
				return ShadowMapSkinnedShader::PerInstanceData{
					.world = mu::transpose(drawEvent.world).getXmf()
					// rootBoneOffset 계산은 앞에서부터 순서대로 이루어져야 하므로 병렬로 수행할 수 없다.
					// 따라서 동기화 후 rootBoneOffset 멤버를 갱신하도록 한다.
				};
			}	
		);

		latch.count_down();
	});
}

// 멀티스레드 작업 시, 드로우콜들에 대해
// 단위 작업을 생성하여 스레드에 할당하는데 사용된다.
void Dispatcher::addJobShadowDraw( ID3D12GraphicsCommandList* threadCmdList,
	const std::vector<DrawEvent>::const_iterator* pItFirst,
	const std::vector<DrawEvent>::const_iterator* pItLast,
	std::size_t firstDrawcallIdx, const ShadowMapData& shadowMapData,
	std::latch& latch
) {
	threadPool_->addJob([=, &latch, &shadowMapData]() {
		// shadow map을 렌더링할 때 쓰는 viewport와 scissor rectangle의 크기는
		// shadow map 텍스처의 크기와 같아야 한다.
		auto shadowViewport = D3D12_VIEWPORT{
			.TopLeftX = 0.f,
			.TopLeftY = 0.f,
			.Width = static_cast<float>(shadowMapData.width),
			.Height = static_cast<float>(shadowMapData.height),
			.MinDepth = 0.f,
			.MaxDepth = 1.f
		};
		auto shadowScissorRect = D3D12_RECT{
			.left = 0, .top = 0,
			.right = static_cast<LONG>(shadowMapData.width),
			.bottom = static_cast<LONG>(shadowMapData.height)
		};

		// 명령 컨텍스트마다 개별적으로 파이프라인 설정을 해주어야 한다.
		// (파이프라인 설정은 공유되지 않는다. 그렇더라.)
		DISPLAY_ERROR_DX_VOID(threadCmdList->SetGraphicsRootSignature(rootSig_->get()), false);
		DISPLAY_ERROR_DX_VOID(threadCmdList->SetPipelineState(shadowShader_.Get()), false);

		auto shadowMapDsv = pDsvPool_->cpuHandle(
			shadowMapData.tex.idxDsv
		);
		DISPLAY_ERROR_DX_VOID( threadCmdList->OMSetRenderTargets(
			0u, nullptr, false, &shadowMapDsv
		), false );

		DISPLAY_ERROR_DX_VOID(threadCmdList->RSSetViewports(1u, &shadowViewport), false);
		DISPLAY_ERROR_DX_VOID(threadCmdList->RSSetScissorRects(1u, &shadowScissorRect), false);

		// bindless 환경 세팅
		// d3d12단 Descriptor Heap 설정
		auto descriptorHeapsRaw = std::vector<ID3D12DescriptorHeap*>(descriptorHeaps_.size());
		std::ranges::transform(descriptorHeaps_, descriptorHeapsRaw.begin(),
			[](ComPtr<ID3D12DescriptorHeap>& comPtrHeap) { return comPtrHeap.Get(); }	
		);
		DISPLAY_ERROR_DX_VOID( threadCmdList->SetDescriptorHeaps(
			static_cast<UINT>(descriptorHeapsRaw.size()), descriptorHeapsRaw.data()
		), false );

		// 별도의 텍스처 샘플링을 하지 않으므로 DescriptorPool 바인딩은 생략한다.
				
		DISPLAY_ERROR_DX_VOID( threadCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST), false );

		// 바인드해야 하는 GPU 데이터는 다음 네 종류다. (셰이더 참고)
		// - PerInstanceData
		// - PerDrawcallData
		// - PerFrameData
		// - BoneData

		// PerInstanceData 바인드
		pResources_->shadowPass.perInstanceData.bind(threadCmdList, rootParamIdxPID_, roomIdx_);
		// PerFrameData 바인드
		pResources_->shadowPass.perFrameData.bind(threadCmdList, rootParamIdxPFD_, roomIdx_);
		// BoneData 바인드
		pResources_->shadowPass.boneData.bind(threadCmdList, rootParamIdxBoneData_, roomIdx_);

		std::size_t idxDrawcall = firstDrawcallIdx;

		// [pItFirst, pItLast]는 각 instancing group의 시작점(혹은 sentinel)이 되는
		// iterator들을 표현한다.
		auto pGroup = pItFirst;

		while (pGroup != pItLast) {
			// [groupFirst, groupLast)는 하나의 instancing group을 표현한다.
			auto groupFirst = *pGroup;
			auto groupLast = *(pGroup + 1);

			const auto& drawEvent = *groupFirst;

			// PerDrawcallData 바인드
			pResources_->shadowPass.perDrawcallData.cbuffers[idxDrawcall].bind(
				threadCmdList, rootParamIdxPDD_, roomIdx_
			);

			auto perDrawcallData = ShadowMapSkinnedShader::PerDrawcallData{
				// perInstanceData에서 현재 instancing group의 첫 번째 인스턴스의 인덱스
				.firstInstanceOffset = static_cast<u32t>(groupFirst - drawEvents_.begin())
			};
			// PerDrawcallData GPU 데이터 갱신
			// (바인드와 GPU 데이터 갱신 순서는 상관없다.
			//  어차피 바인드는 GPU 명령이라 바로 실행되지 않기 때문에)
			pResources_->shadowPass.perDrawcallData.cbuffers[idxDrawcall].stage(
				roomIdx_, &perDrawcallData, 1u
			);

			layoutMeshIfNeeded(*drawEvent.mesh);
			auto& vbViews = drawEvent.mesh->vbViewsByPipeline.at("PBRSkinnedPipeline_Shadow");

			DISPLAY_ERROR_DX_VOID( threadCmdList->IASetVertexBuffers(
				0u, static_cast<UINT>(vbViews.size()), vbViews.data()
			), false );
			DISPLAY_ERROR_DX_VOID( threadCmdList->IASetIndexBuffer(&drawEvent.subMesh->ibView), false );

			const auto indexStride = drawEvent.subMesh->ibView.Format == DXGI_FORMAT_R16_UINT
				? sizeof(u16t) : sizeof(u32t);

			DISPLAY_ERROR_DX_VOID( threadCmdList->DrawIndexedInstanced(
				static_cast<UINT>(drawEvent.subMesh->ibView.SizeInBytes / indexStride),
				static_cast<UINT>(groupLast - groupFirst), 0u, 0, 0u
			), false );

			++idxDrawcall;
			++pGroup;
		}

		// 명령 기록 종료
		DISPLAY_ERROR_DX_HR( threadCmdList->Close(), false );
		latch.count_down();
	} );
}

}	// namespace PBRSkinnedPipeline