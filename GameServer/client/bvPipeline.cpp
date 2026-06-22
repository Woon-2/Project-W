#include "pch.hpp"
#include "bvPipeline.hpp"
#include "shader.hpp"
#include "errorHandling.hpp"

namespace BVPipeline {

namespace Detail {
	ComPtr<ID3D12Resource> staticVBCube;
	D3D12_VERTEX_BUFFER_VIEW staticVBViewCube;
	ComPtr<ID3D12Resource> staticIBCube;
	D3D12_INDEX_BUFFER_VIEW staticIBViewCube;
}	// namespace BVPipeline::Detail

void initStaticModels(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, Fence& fenceToAssociate) {
	static const auto positions = std::vector<XMFLOAT3>{
        XMFLOAT3(-0.5f,-0.5f,-0.5f),    // triangle 1
        XMFLOAT3(-0.5f,-0.5f, 0.5f),
        XMFLOAT3(-0.5f, 0.5f, 0.5f),
        XMFLOAT3(0.5f, 0.5f,-0.5f),     // triangle 2 ...
        XMFLOAT3(-0.5f,-0.5f,-0.5f),
        XMFLOAT3(-0.5f, 0.5f,-0.5f), 
        XMFLOAT3(0.5f,-0.5f, 0.5f),     
        XMFLOAT3(-0.5f,-0.5f,-0.5f),
        XMFLOAT3(0.5f,-0.5f,-0.5f),
        XMFLOAT3(0.5f, 0.5f,-0.5f),     
        XMFLOAT3(0.5f,-0.5f,-0.5f),
        XMFLOAT3(-0.5f,-0.5f,-0.5f),
        XMFLOAT3(-0.5f,-0.5f,-0.5f),    
        XMFLOAT3(-0.5f, 0.5f, 0.5f),
        XMFLOAT3(-0.5f, 0.5f,-0.5f),
        XMFLOAT3(0.5f,-0.5f, 0.5f),     
        XMFLOAT3(-0.5f,-0.5f, 0.5f),
        XMFLOAT3(-0.5f,-0.5f,-0.5f),
        XMFLOAT3(-0.5f, 0.5f, 0.5f),    
        XMFLOAT3(-0.5f,-0.5f, 0.5f),
        XMFLOAT3(0.5f,-0.5f, 0.5f),
        XMFLOAT3(0.5f, 0.5f, 0.5f),     
        XMFLOAT3(0.5f,-0.5f,-0.5f),
        XMFLOAT3(0.5f, 0.5f,-0.5f),
        XMFLOAT3(0.5f,-0.5f,-0.5f),     
        XMFLOAT3(0.5f, 0.5f, 0.5f),
        XMFLOAT3(0.5f,-0.5f, 0.5f),
        XMFLOAT3(0.5f, 0.5f, 0.5f),     
        XMFLOAT3(0.5f, 0.5f,-0.5f),
        XMFLOAT3(-0.5f, 0.5f,-0.5f),
        XMFLOAT3(0.5f, 0.5f, 0.5f),     
        XMFLOAT3(-0.5f, 0.5f,-0.5f),
        XMFLOAT3(-0.5f, 0.5f, 0.5f),
        XMFLOAT3(0.5f, 0.5f, 0.5f),     
        XMFLOAT3(-0.5f, 0.5f, 0.5f),
        XMFLOAT3(0.5f,-0.5f, 0.5f)
    };

    static const auto indices = std::vector<u16t>{
        0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u, 17u,
        18u, 19u, 20u, 21u, 22u, 23u, 24u, 25u, 26u, 27u, 28u, 29u, 30u, 31u, 32u, 33u, 34u, 35u
    };

    // 정점 버퍼들 구축
    // 속성마다 별도의 버퍼를 만든다.
	auto vbPosition = createBufferResource(device, nullptr, positions.size() * sizeof(XMFLOAT3), BufferCreationType::VertexBuffer);
    setD3DName(vbPosition.Get(), "CubeMesh_VB_Position");
	auto vbPositionu = createBufferResource(device, positions.data(), positions.size() * sizeof(XMFLOAT3), BufferCreationType::UploadBuffer);
    setD3DName(vbPositionu.Get(), "CubeMesh_VB_Position_Upload");

	copyResource( cmdList, vbPositionu.Get(), vbPosition.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	);

    // 인덱스 버퍼 구축
	auto ib = createBufferResource(device, nullptr, indices.size() * sizeof(u16t), BufferCreationType::IndexBuffer);
    setD3DName(ib.Get(), "CubeMesh_IB");
	auto ibu = createBufferResource(device, indices.data(), indices.size() * sizeof(u16t), BufferCreationType::UploadBuffer);
    setD3DName(ibu.Get(), "CubeMesh_IB_Upload");

	copyResource( cmdList, ibu.Get(), ib.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_STATE_INDEX_BUFFER
	);

	Detail::staticVBViewCube = D3D12_VERTEX_BUFFER_VIEW{
		.BufferLocation = vbPosition->GetGPUVirtualAddress(),
		.SizeInBytes = static_cast<UINT>(positions.size() * sizeof(XMFLOAT3)),
		.StrideInBytes = static_cast<UINT>(sizeof(XMFLOAT3))
	};

	Detail::staticIBViewCube = D3D12_INDEX_BUFFER_VIEW{
		.BufferLocation = ib->GetGPUVirtualAddress(),
		.SizeInBytes = static_cast<UINT>(indices.size() * sizeof(u16t)),
		.Format = DXGI_FORMAT_R16_UINT
	};

	Detail::staticVBCube = std::move(vbPosition);
	Detail::staticIBCube = std::move(ib);

    gSharedLog << "[Resource Load] BVPipeline Static Cube Mesh 구축 완료\n";

    fenceToAssociate.associatedResources_.push_back(std::move(vbPositionu));
    fenceToAssociate.associatedResources_.push_back(std::move(ibu));
}

// GFX 객체로부터 필요한 인자들을 전달받자.
Dispatcher::Dispatcher(
	const std::vector<ComPtr<ID3D12DescriptorHeap>>& descriptorHeaps,
	const std::shared_ptr<RootSig>& rootSig,
	const ComPtr<ID3D12PipelineState>& shader,
	RenderSubmitter* submitter,
	const D3D12_VIEWPORT& viewport,
	const D3D12_RECT& scissorRect, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
	D3D12_CPU_DESCRIPTOR_HANDLE dsv, Fence* pFence,
	Resources* pResources, ThreadPool* threadPool,
	CommandListPool* commandListPool,
	std::vector<DrawEvent>&& drawEvents,
	const CameraData& cameraData,
	std::size_t roomIdx
) : descriptorHeaps_(descriptorHeaps), rootSig_(rootSig), shader_(shader), submitter_(submitter),
	viewport_(viewport), scissorRect_(scissorRect), rtv_(rtv), dsv_(dsv), pFence_(pFence),
	pResources_(pResources), threadPool_(threadPool), cmdListPool_(commandListPool), drawEvents_(std::move(drawEvents)),
	cameraData_(cameraData), roomIdx_(roomIdx),
	rootParamIdxPDD_(rootSig->paramIdx("PerDrawcallData")),
	rootParamIdxPID_(rootSig->paramIdx("PerInstanceData")) {}

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
	static auto perInstanceData = std::vector<BVShader::PerInstanceData>();
	perInstanceData.resize(drawEvents_.size());

	const auto viewProj = cameraData_.view * cameraData_.proj;

	// DrawEvents에 담겨있는 정보를 가공해 perInstanceData에 저장한다.
	std::ranges::transform(drawEvents_, perInstanceData.begin(),
		[viewProj](const BVPipeline::DrawEvent& drawEvent) {
			return BVShader::PerInstanceData{
				.wvp   = mu::transpose(drawEvent.world * viewProj).getXmf(),
				.color = drawEvent.color.getXmf()
			};
		}
	);

	// perInstanceData의 내용을 바탕으로 GPU 데이터를 갱신한다.
	pResources_->perInstanceData.stage(roomIdx_, perInstanceData);
	perInstanceData.clear();
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
	static auto perInstanceData = std::vector<BVShader::PerInstanceData>();
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
		addJobUpdate( viewProj, drawEvents_.data() + accEventCnt,
			drawEvents_.data() + accEventCnt + jobSizeUpdate_,
			perInstanceData.data() + accEventCnt, latch
		);

		accEventCnt += jobSizeUpdate_;
	}
	
	// 찌꺼기 처리
	if (accEventCnt != drawEvents_.size()) {
		const auto lastJobSize = drawEvents_.size() - accEventCnt;

		addJobUpdate( viewProj, drawEvents_.data() + accEventCnt,
			drawEvents_.data() + accEventCnt + lastJobSize,
			perInstanceData.data() + accEventCnt, latch
		);
	}

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
	// d3d12단 Descriptor Heap 설정
	auto descriptorHeapsRaw = std::vector<ID3D12DescriptorHeap*>(descriptorHeaps_.size());
	std::ranges::transform(descriptorHeaps_, descriptorHeapsRaw.begin(),
		[](ComPtr<ID3D12DescriptorHeap>& comPtrHeap) { return comPtrHeap.Get(); }	
	);
	DISPLAY_ERROR_DX_VOID( cmdList->SetDescriptorHeaps(
		static_cast<UINT>(descriptorHeapsRaw.size()), descriptorHeapsRaw.data()
	), false );

	// 텍스처 샘플링을 하지 않으므로 그와 관련된 바인딩은 따로 하지 않는다.

	DISPLAY_ERROR_DX_VOID( cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST), false );

	// 바인드해야 하는 GPU 데이터는 다음 두 종류다. (셰이더 참고)
	// - PerInstanceData
	// - PerDrawcallData

	// PerInstanceData 바인드
	pResources_->perInstanceData.bind(cmdList, rootParamIdxPID_, roomIdx_);

	u32t idxDrawcall = 0u;

	// 인스턴싱을 적용한다.
	// equivalent하게 평가되는 DrawEvent들을 (같은 Bounding Volume Model 사용)
	// 묶어서 instancing group으로 삼아 하나의 드로우콜로 처리한다.
	// 
	// [groupFirst, groupLast)는 하나의 instancing group을 표현한다.
	auto groupFirst = drawEvents_.begin();
	while (groupFirst != drawEvents_.end()) {
		auto& drawEvent = *groupFirst;

		auto groupLast = std::upper_bound(groupFirst, drawEvents_.end(), drawEvent);

		// PerDrawcallData 바인드
		pResources_->perDrawcallData.cbuffers[idxDrawcall].bind(
			cmdList, rootParamIdxPDD_, roomIdx_
		);

		// PerDrawcallData GPU 데이터 갱신
		// (바인드와 GPU 데이터 갱신 순서는 상관없다.
		//  어차피 바인드는 GPU 명령이라 바로 실행되지 않기 때문에)
		auto perDrawcallData = BVShader::PerDrawcallData{
			// perInstanceData에서 현재 instancing group의 첫 번째 인스턴스의 인덱스
			.firstInstanceOffset = static_cast<u32t>(groupFirst - drawEvents_.begin())
		};
		pResources_->perDrawcallData.cbuffers[idxDrawcall].stage(
			roomIdx_, &perDrawcallData, 1u
		);

		switch (drawEvent.bvModel) {
		case BVModel::Box:
			DISPLAY_ERROR_DX_VOID( cmdList->IASetVertexBuffers(
				0u, 1u, &Detail::staticVBViewCube
			), false );
			DISPLAY_ERROR_DX_VOID( cmdList->IASetIndexBuffer(&Detail::staticIBViewCube), false );

			DISPLAY_ERROR_DX_VOID( cmdList->DrawIndexedInstanced(
				static_cast<UINT>(Detail::staticIBViewCube.SizeInBytes / sizeof(u16t)),
				static_cast<UINT>(groupLast - groupFirst), 0u, 0, 0u
			), false );
			break;

		default:
			DISPLAY_ERROR_STR(false, "[GFX Error] BVPipeline::Dispatcher::drawSingleThreaded: "s
				+ "렌더링이 준비되어 있지 않은 종류의 BVModel 값을 읽었습니다.", false
			)
			break;
		}

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

	DISPLAY_ERROR_DX_VOID(submitter_->submit(1u, stagedCmdLists), false);
	
	// Fence 객체에 사용한 명령 컨텍스트를 연관시켜 놓는다.
	pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)]
		.push_back(std::move(cmdCtx));
}

// DrawEvents의 정보들을 참고하여
// 드로우콜들을 수행한다.
// 멀티스레드로 동작한다.
// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
void Dispatcher::drawMultiThreaded() {
	if (drawEvents_.empty()) {
		return;
	}

	// 인스턴싱을 적용한다.
	// equivalent하게 평가되는 DrawEvent들을 (같은 Bounding Volume Model 사용)
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
		addJobDraw( currCmdCtx->cmdList.Get(), instancingGroups.data() + accDrawcallCnt,
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
		addJobDraw( currCmdCtx->cmdList.Get(), instancingGroups.data() + accDrawcallCnt,
			instancingGroups.data() + accDrawcallCnt + lastJobSize, accDrawcallCnt, latch
		);
	}

	// 동기화
	latch.wait();

	instancingGroups.clear();

	// 명령 기록 끝, 실행
	auto stagedCmdLists = std::vector<ID3D12CommandList*>(cmdCtxs.size(), nullptr);
	std::ranges::transform(cmdCtxs, stagedCmdLists.begin(),
		[](const CommandContext& cmdCtx) { return cmdCtx.cmdList.Get(); }	
	);

	DISPLAY_ERROR_DX_VOID( submitter_->submit(
		static_cast<UINT>(stagedCmdLists.size()), stagedCmdLists.data()
	), false );

	// Fence 객체에 사용한 명령 컨텍스트들을 연관시켜 놓는다.
	pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)]
		.splice( pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].end(), std::move(cmdCtxs) );
}

// 멀티스레드 작업 시, GPU 데이터 갱신 작업에 대해
// 단위 작업을 생성하여 스레드에 할당하는데 사용된다.
void MU_CALLCONV Dispatcher::addJobUpdate( mu::Mat4x4 viewProj,
	const DrawEvent* pFirst, const DrawEvent* pLast, BVShader::PerInstanceData* pOut,
	std::latch& latch
) {
	threadPool_->addJob( [=, &latch](){
		std::transform( pFirst, pLast, pOut,
			[viewProj](const BVPipeline::DrawEvent& drawEvent) {
				return BVShader::PerInstanceData{
					.wvp   = mu::transpose(drawEvent.world * viewProj).getXmf(),
					.color = drawEvent.color.getXmf()
				};
			}
		);

		latch.count_down();
	});
}

// 멀티스레드 작업 시, 드로우콜들에 대해
// 단위 작업을 생성하여 스레드에 할당하는데 사용된다.
void Dispatcher::addJobDraw( ID3D12GraphicsCommandList* threadCmdList,
	const std::vector<DrawEvent>::const_iterator* pItFirst,
	const std::vector<DrawEvent>::const_iterator* pItLast,
	std::size_t firstDrawcallIdx, std::latch& latch
) {
	threadPool_->addJob([=, &latch]() {
		// 명령 컨텍스트마다 개별적으로 파이프라인 설정을 해주어야 한다.
		// (파이프라인 설정은 공유되지 않는다. 그렇더라.)
		DISPLAY_ERROR_DX_VOID(threadCmdList->SetGraphicsRootSignature(rootSig_->get()), false);
		DISPLAY_ERROR_DX_VOID(threadCmdList->SetPipelineState(shader_.Get()), false);
		DISPLAY_ERROR_DX_VOID(threadCmdList->OMSetRenderTargets(1u, &rtv_, false, &dsv_), false);
		DISPLAY_ERROR_DX_VOID(threadCmdList->RSSetViewports(1u, &viewport_), false);
		DISPLAY_ERROR_DX_VOID(threadCmdList->RSSetScissorRects(1u, &scissorRect_), false);

		// bindless 환경 세팅
		// d3d12단 Descriptor Heap 설정
		auto descriptorHeapsRaw = std::vector<ID3D12DescriptorHeap*>(descriptorHeaps_.size());
		std::ranges::transform(descriptorHeaps_, descriptorHeapsRaw.begin(),
			[](ComPtr<ID3D12DescriptorHeap>& comPtrHeap) { return comPtrHeap.Get(); }	
		);
		DISPLAY_ERROR_DX_VOID( threadCmdList->SetDescriptorHeaps(
			static_cast<UINT>(descriptorHeapsRaw.size()), descriptorHeapsRaw.data()
		), false );

		// 텍스처 샘플링을 하지 않으므로 그와 관련된 바인딩은 따로 하지 않는다.
				
		DISPLAY_ERROR_DX_VOID( threadCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST), false );

		// 바인드해야 하는 GPU 데이터는 다음 두 종류다. (셰이더 참고)
		// - PerInstanceData
		// - PerDrawcallData

		// PerInstanceData 바인드
		pResources_->perInstanceData.bind(threadCmdList, rootParamIdxPID_, roomIdx_);

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
			pResources_->perDrawcallData.cbuffers[idxDrawcall].bind(
				threadCmdList, rootParamIdxPDD_, roomIdx_
			);

			auto perDrawcallData = BVShader::PerDrawcallData{
				// perInstanceData에서 현재 instancing group의 첫 번째 인스턴스의 인덱스
				.firstInstanceOffset = static_cast<u32t>(groupFirst - drawEvents_.begin())
			};
			// PerDrawcallData GPU 데이터 갱신
			// (바인드와 GPU 데이터 갱신 순서는 상관없다.
			//  어차피 바인드는 GPU 명령이라 바로 실행되지 않기 때문에)
			pResources_->perDrawcallData.cbuffers[idxDrawcall].stage(
				roomIdx_, &perDrawcallData, 1u
			);

			switch (drawEvent.bvModel) {
			case BVModel::Box:
				DISPLAY_ERROR_DX_VOID( threadCmdList->IASetVertexBuffers(
					0u, 1u, &Detail::staticVBViewCube
				), false );
				DISPLAY_ERROR_DX_VOID( threadCmdList->IASetIndexBuffer(&Detail::staticIBViewCube), false );

				DISPLAY_ERROR_DX_VOID( threadCmdList->DrawIndexedInstanced(
					static_cast<UINT>(Detail::staticIBViewCube.SizeInBytes / sizeof(u16t)),
					static_cast<UINT>(groupLast - groupFirst), 0u, 0, 0u
				), false );
				break;

			default:
				DISPLAY_ERROR_STR(false, "[GFX Error] BVPipeline::Dispatcher::drawSingleThreaded: "s
					+ "렌더링이 준비되어 있지 않은 종류의 BVModel 값을 읽었습니다.", false
				)
				break;
			}

			++idxDrawcall;
			++pGroup;
		}

		// 명령 기록 종료
		DISPLAY_ERROR_DX_HR( threadCmdList->Close(), false );
		latch.count_down();
	} );
}

}	// namespace BVPipeline