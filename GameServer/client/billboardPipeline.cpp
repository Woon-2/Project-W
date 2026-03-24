#include "pch.hpp"
#include "billboardPipeline.hpp"
#include "shader.hpp"
#include "mesh.hpp"
#include "errorHandling.hpp"

namespace BillboardPipeline {

namespace Detail {
	std::vector<ComPtr<ID3D12Resource>> staticVBPoints;
	std::vector<D3D12_VERTEX_BUFFER_VIEW> staticVBViewPoints;
	std::map<std::string, u32t> staticVBIdxMapPoints;
	ComPtr<ID3D12Resource> staticIBPoint;
	D3D12_INDEX_BUFFER_VIEW staticIBViewPoint;
}	// namespace BillboardPipeline::Detail

void initStaticPointMesh( ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, Fence& fenceToAssociate ) {
	static const auto positions = std::vector<XMFLOAT3>{
	XMFLOAT3( 0.f, 0.f, 0.f )
	};

	static const auto sizes = std::vector<float>{
		1.f
	};

	static const auto indices = std::vector<u16t>{
		0u
	};

	// 정점 버퍼들 구축
	auto vbPosition = createBufferResource( device, nullptr, positions.size() * sizeof( XMFLOAT3 ), BufferCreationType::VertexBuffer );
	setD3DName( vbPosition.Get(), "PointMesh_VB_Position" );
	auto vbPositionu = createBufferResource( device, positions.data(), positions.size() * sizeof( XMFLOAT3 ), BufferCreationType::UploadBuffer );
	setD3DName( vbPositionu.Get(), "PointMesh_VB_Position_Upload" );

	copyResource( cmdList, vbPositionu.Get(), vbPosition.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	);

	auto vbSize = createBufferResource( device, nullptr, sizes.size() * sizeof( float ), BufferCreationType::VertexBuffer );
	setD3DName( vbSize.Get(), "PointMesh_VB_Size" );
	auto vbSizeu = createBufferResource( device, sizes.data(), sizes.size() * sizeof( float ), BufferCreationType::UploadBuffer );
	setD3DName( vbSizeu.Get(), "PointMesh_VB_Size_Upload" );

	copyResource( cmdList, vbSizeu.Get(), vbSize.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	);

	// 인덱스 버퍼 구축
	auto ib = createBufferResource( device, nullptr, indices.size() * sizeof( u16t ), BufferCreationType::IndexBuffer );
	setD3DName( ib.Get(), "PointMesh_IB" );
	auto ibu = createBufferResource( device, indices.data(), indices.size() * sizeof( u16t ), BufferCreationType::UploadBuffer );
	setD3DName( ibu.Get(), "PointMesh_IB_Upload" );

	copyResource( cmdList, ibu.Get(), ib.Get(), D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_STATE_INDEX_BUFFER
	);

	// 만든 버퍼들을 조합하여 메시 구축
	// Vertex Buffer View 구성
	Detail::staticVBViewPoints.emplace_back(
		/* .BufferLocation = */ vbPosition->GetGPUVirtualAddress(),
		/* .SizeInBytes = */ static_cast<UINT>(positions.size() * sizeof( XMFLOAT3 )),
		/* .StrideInBytes = */ static_cast<UINT>(sizeof( XMFLOAT3 ))
	);

	Detail::staticVBViewPoints.emplace_back(
		/* .BufferLocation = */ vbSize->GetGPUVirtualAddress(),
		/* .SizeInBytes = */ static_cast<UINT>(sizes.size() * sizeof( float )),
		/* .StrideInBytes = */ static_cast<UINT>(sizeof( float ))
	);

	// SubMesh 구성
	Detail::staticIBViewPoint = D3D12_INDEX_BUFFER_VIEW{
		.BufferLocation = ib->GetGPUVirtualAddress(),
		.SizeInBytes = static_cast<UINT>(indices.size() * sizeof( u16t )),
		.Format = DXGI_FORMAT_R16_UINT
	};

	// 자료구조 등록 (Vertex Buffer View와 SubMesh는 위에서 등록하였음)
	Detail::staticVBPoints.push_back( std::move( vbPosition ) );
	Detail::staticVBIdxMapPoints.try_emplace( "PointMesh_VB_Position", 0u );
	Detail::staticVBPoints.push_back( std::move( vbSize ) );
	Detail::staticVBIdxMapPoints.try_emplace( "PointMesh_VB_Size", 1u );
	Detail::staticIBPoint = std::move( ib );

	gSharedLog << "[Resource Load] BillboardPipeline Static Point Mesh 구축 완료\n";

	fenceToAssociate.associatedResources_.push_back( std::move( vbPositionu ) );
	fenceToAssociate.associatedResources_.push_back( std::move( vbSizeu ) );
	fenceToAssociate.associatedResources_.push_back( std::move( ibu ) );
}

// GFX 객체로부터 필요한 인자들을 전달받자.
Dispatcher::Dispatcher(
	const std::vector<ComPtr<ID3D12DescriptorHeap>>& descriptorHeaps,
	DescriptorPool* pTexPool, DescriptorPool* pTexArrayPool,
	DescriptorPool* pTexCubePool, DescriptorPool* pSamPool,
	DescriptorPool* pCmpSamPool,
	const std::shared_ptr<RootSig>& rootSig, const ComPtr<ID3D12PipelineState>& shader,
	const ComPtr<ID3D12PipelineState>& shaderAdditive,
	const ComPtr<ID3D12CommandQueue>& cmdQ, const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissorRect,
	D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv, Fence* pFence, Resources* pResources,
	ThreadPool* threadPool, CommandListPool* commandListPool, std::vector<DrawEvent>&& drawEvents,
	const CameraData& cameraData, const FrameData& frameData, std::size_t roomIdx
) : descriptorHeaps_(descriptorHeaps), pTexPool_(pTexPool), pTexArrayPool_(pTexArrayPool),
	pTexCubePool_(pTexCubePool), pSamPool_(pSamPool), pCmpSamPool_(pCmpSamPool),
	rootSig_(rootSig), shader_(shader), shaderAdditive_(shaderAdditive), cmdQ_(cmdQ),
	viewport_(viewport), scissorRect_(scissorRect), rtv_(rtv), dsv_(dsv), pFence_(pFence),
	pResources_(pResources), threadPool_(threadPool), cmdListPool_(commandListPool), drawEvents_(std::move(drawEvents)),
	cameraData_(cameraData), frameData_( frameData ), roomIdx_(roomIdx),
	rootParamIdxPDD_(rootSig->paramIdx("PerDrawcallData")),
	rootParamIdxPID_(rootSig->paramIdx("PerInstanceData")),
	rootParamIdxPFD_( rootSig->paramIdx( "PerFrameData" ) ),
	rootParamIdxTexPool_(rootSig->paramIdx("TexturePool")),
	rootParamIdxTexArrayPool_(rootSig->paramIdx("TextureArrayPool")),
	rootParamIdxTexCubePool_(rootSig->paramIdx("TextureCubePool")),
	rootParamIdxSamPool_(rootSig->paramIdx("SamplerPool")),
	rootParamIdxCmpSamPool_( rootSig->paramIdx( "ComparisonSamplerPool" ) ) {}

// 셰이더에서 사용하는 GPU 데이터를 갱신한다.
// DrawEvents, CameraData에 담겨있는 정보를 가공하여
// Resources 객체에 담긴,
// ShaderInputBuffer 인터페이스를 가지는 객체들에 옮겨담는다.
// 싱글스레드로 동작한다.
// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
void Dispatcher::updateGPUDataSingleThreaded() {
	if ( drawEvents_.empty() ) {
		return;
	}

	// 메시 데이터 업로드
	// 정렬을 통해 인스턴싱이 가능하도록 한다.
	std::sort( drawEvents_.begin(), drawEvents_.end() );

	// perInstanceData를 static으로 선언하여
	// 매번 처음부터 메모리를 구축하지 않고 재사용할 수 있도록 한다.
	static auto perInstanceData = std::vector<BillboardShader::PerInstanceData>();
	perInstanceData.resize( drawEvents_.size() );

	const auto viewProj = cameraData_.view * cameraData_.proj;
	const auto cameraPosV = cameraData_.pos;

	// DrawEvents에 담겨있는 정보를 가공해 perInstanceData에 저장한다.
	std::ranges::transform( drawEvents_, perInstanceData.begin(),
		[]( const BillboardPipeline::DrawEvent& drawEvent ) {
			return BillboardShader::PerInstanceData{
				.world    = mu::transpose( drawEvent.world ).getXmf(),
				.rotation = drawEvent.rotation,
			};
		}
	);

	// perInstanceData의 내용을 바탕으로 GPU 데이터를 갱신한다.
	pResources_->perInstanceData.stage( roomIdx_, perInstanceData );
	perInstanceData.clear();

	// FrameData에 담겨있는 정보를 가공해 pfd에 저장한다.
	auto pfd = BillboardShader::PerFrameData{
		.vp = mu::transpose( viewProj ).getXmf(),
		.cameraPosV = cameraPosV.getXmf()
	};
	// pfd의 내용을 바탕으로 GPU 데이터를 갱신한다.
	pResources_->perFrameData.stage( roomIdx_, &pfd, 1u );
}

// 셰이더에서 사용하는 GPU 데이터를 갱신한다.
// DrawEvents, CameraData에 담겨있는 정보를 가공하여
// Resources 객체에 담긴,
// ShaderInputBuffer 인터페이스를 가지는 객체들에 옮겨담는다.
// 싱글스레드로 동작한다.
// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
void Dispatcher::updateGPUDataMultiThreaded() {
	if ( drawEvents_.empty() ) {
		return;
	}

	// 메시 데이터 업로드
	// 정렬을 통해 인스턴싱이 가능하도록 한다.
	std::sort( drawEvents_.begin(), drawEvents_.end() );

	// perInstanceData를 static으로 선언하여
	// 매번 처음부터 메모리를 구축하지 않고 재사용할 수 있도록 한다.
	static auto perInstanceData = std::vector<BillboardShader::PerInstanceData>();
	perInstanceData.resize( drawEvents_.size() );

	// DrawEvent는 jobSize 단위로 스레드들에 분배될 것이므로,
	// 동기화를 위한 latch를 준비한다.
	// 이때, DrawEvent의 개수가 jobSize로 나누어 떨어지지 않을 경우를 대비한다.
	auto latch = std::latch( drawEvents_.size() / jobSizeUpdate_
		+ ((drawEvents_.size() % jobSizeUpdate_) != 0)
	);

	const auto viewProj = cameraData_.view * cameraData_.proj;
	const auto cameraPosV = cameraData_.pos;

	// drawEvents의 [accEventCnt, accEventCnt + jobSizeUpdate_) 범위의
	// 데이터를 가공해 perInstanceData의 대응되는 영역에 저장한다.
	std::size_t accEventCnt = 0u;
	while ( accEventCnt + (jobSizeUpdate_ - 1) < drawEvents_.size() ) {
		addJobUpdate( viewProj, drawEvents_.data() + accEventCnt,
			drawEvents_.data() + accEventCnt + jobSizeUpdate_,
			perInstanceData.data() + accEventCnt, latch
		);

		accEventCnt += jobSizeUpdate_;
	}

	// 찌꺼기 처리
	if ( accEventCnt != drawEvents_.size() ) {
		const auto lastJobSize = drawEvents_.size() - accEventCnt;

		addJobUpdate( viewProj, drawEvents_.data() + accEventCnt,
			drawEvents_.data() + accEventCnt + lastJobSize,
			perInstanceData.data() + accEventCnt, latch
		);
	}

	// FrameData에 담겨있는 정보를 가공해 pfd에 저장한다.
	auto pfd = BillboardShader::PerFrameData{
		.vp = mu::transpose( viewProj ).getXmf(),
		.cameraPosV = cameraPosV.getXmf()
	};
	// pfd의 내용을 바탕으로 GPU 데이터를 갱신한다.
	pResources_->perFrameData.stage( roomIdx_, &pfd, 1u );

	// 동기화
	latch.wait();
	// 모든 데이터가 가공된 이후, GPU 데이터를 갱신한다.
	pResources_->perInstanceData.stage( roomIdx_, perInstanceData );
	perInstanceData.clear();
}

// DrawEvents의 정보들을 참고하여
// 드로우콜들을 수행한다.
// 싱글스레드로 동작한다.
// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
void Dispatcher::drawSingleThreaded() {
	if ( drawEvents_.empty() ) {
		return;
	}

	// 명령 컨텍스트 할당
	CommandContext cmdCtx{};
	DISPLAY_ERROR_STR( cmdListPool_->allocOne( CommandListUsage::RenderingSlave, cmdCtx ),
		"[GFX Error] GFX::drawSingleThreaded: 요청한 명령 리스트를 할당받지 못했습니다.", false
	);
	if ( !cmdCtx.cmdList ) {
		return;
	}

	// 명령 컨텍스트 초기화
	auto cmdList = cmdCtx.cmdList.Get();
	auto cmdAlloc = cmdCtx.cmdAlloc.Get();
	auto hrCmdAllocReset = cmdAlloc->Reset();
	DISPLAY_ERROR_DX_HR( hrCmdAllocReset, false );
	if ( hrCmdAllocReset < 0 ) {
		cmdListPool_->freeOne( CommandListUsage::RenderingSlave, std::move( cmdCtx ) );
		return;
	}
	auto hrCmdListReset = cmdList->Reset( cmdAlloc, nullptr );
	DISPLAY_ERROR_DX_HR( hrCmdListReset, false );
	if ( hrCmdListReset < 0 ) {
		cmdListPool_->freeOne( CommandListUsage::RenderingSlave, std::move( cmdCtx ) );
		return;
	}

	// 명령 기록 시작 — 첫 PSO는 비가산(non-additive)
	DISPLAY_ERROR_DX_VOID( cmdList->SetGraphicsRootSignature( rootSig_->get() ), false );
	DISPLAY_ERROR_DX_VOID( cmdList->SetPipelineState( shader_.Get() ), false );
	DISPLAY_ERROR_DX_VOID( cmdList->OMSetRenderTargets( 1u, &rtv_, false, &dsv_ ), false );
	DISPLAY_ERROR_DX_VOID( cmdList->RSSetViewports( 1u, &viewport_ ), false );
	DISPLAY_ERROR_DX_VOID( cmdList->RSSetScissorRects( 1u, &scissorRect_ ), false );

	// bindless 환경 세팅
	// d3d12단 Descriptor Heap, Descriptor Table 설정
	auto descriptorHeapsRaw = std::vector<ID3D12DescriptorHeap*>( descriptorHeaps_.size() );
	std::ranges::transform( descriptorHeaps_, descriptorHeapsRaw.begin(),
		[]( ComPtr<ID3D12DescriptorHeap>& comPtrHeap ) { return comPtrHeap.Get(); }
	);
	DISPLAY_ERROR_DX_VOID( cmdList->SetDescriptorHeaps(
		static_cast<UINT>(descriptorHeapsRaw.size()), descriptorHeapsRaw.data()
	), false );

	pTexPool_->bind( cmdList, rootParamIdxTexPool_ );
	pTexArrayPool_->bind( cmdList, rootParamIdxTexArrayPool_ );
	pTexCubePool_->bind( cmdList, rootParamIdxTexCubePool_ );
	pSamPool_->bind( cmdList, rootParamIdxSamPool_ );
	pCmpSamPool_->bind( cmdList, rootParamIdxCmpSamPool_ );

	DISPLAY_ERROR_DX_VOID( cmdList->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_POINTLIST ), false );

	// 바인드해야 하는 GPU 데이터는 다음 세 종류다. (셰이더 참고)
	// - PerInstanceData
	// - PerDrawcallData
	// - PerFrameData

	// PerInstanceData 바인드
	pResources_->perInstanceData.bind( cmdList, rootParamIdxPID_, roomIdx_ );
	// PerFrameData 바인드
	pResources_->perFrameData.bind( cmdList, rootParamIdxPFD_, roomIdx_ );

	u32t idxDrawcall = 0u;
	bool currentAdditive = false;

	// DrawEvent들을 하나씩 처리한다. (정렬 후: non-additive → additive 순)
	for ( const auto& drawEvent : drawEvents_ ) {
		// additive 경계에서 PSO 전환
		if ( drawEvent.additive != currentAdditive ) {
			currentAdditive = drawEvent.additive;
			auto* pso = currentAdditive ? shaderAdditive_.Get() : shader_.Get();
			DISPLAY_ERROR_DX_VOID( cmdList->SetPipelineState( pso ), false );
		}

		// PerDrawcallData 바인드
		pResources_->perDrawcallData.cbuffers[idxDrawcall].bind(
			cmdList, rootParamIdxPDD_, roomIdx_
		);

		// PerDrawcallData GPU 데이터 갱신
		// (바인드와 GPU 데이터 갱신 순서는 상관없다.
		//  어차피 바인드는 GPU 명령이라 바로 실행되지 않기 때문에)
		auto perDrawcallData = BillboardShader::PerDrawcallData{
			.material = BillboardShader::Material{
				.idxTex = drawEvent.pTex->idxSrv,
				.tint = drawEvent.tint.getXmf()
			},
			.firstInstanceOffset = idxDrawcall
		};
		pResources_->perDrawcallData.cbuffers[idxDrawcall].stage(
			roomIdx_, &perDrawcallData, 1u
		);

		DISPLAY_ERROR_DX_VOID( 
			cmdList->IASetVertexBuffers( 0u, static_cast<UINT>(Detail::staticVBViewPoints.size()),
				Detail::staticVBViewPoints.data()
			),	false 
		);
		DISPLAY_ERROR_DX_VOID( cmdList->IASetIndexBuffer( &Detail::staticIBViewPoint ), false );

		DISPLAY_ERROR_DX_VOID( cmdList->DrawIndexedInstanced(
			static_cast<UINT>(Detail::staticIBViewPoint.SizeInBytes / sizeof( u16t )),
			1u, 0u, 0, 0u
		), false );

		++idxDrawcall;
	}

	auto hrClose = cmdList->Close();
	DISPLAY_ERROR_DX_HR( hrClose, false );
	if ( hrClose < 0 ) {
		cmdListPool_->freeOne( CommandListUsage::RenderingSlave, std::move( cmdCtx ) );
		return;
	}

	// 명령 기록 끝, 실행
	ID3D12CommandList* stagedCmdLists[] = { cmdList };

	DISPLAY_ERROR_DX_VOID( cmdQ_->ExecuteCommandLists( 1u, stagedCmdLists ), false );

	// Fence 객체에 사용한 명령 컨텍스트를 연관시켜 놓는다.
	pFence_->associatedCmdCtxs_[etoi( CommandListUsage::RenderingSlave )]
		.push_back( std::move( cmdCtx ) );
}

// DrawEvents의 정보들을 참고하여
// 드로우콜들을 수행한다.
// 멀티스레드로 동작한다.
// DrawEvents가 비어있다면 아무 동작도 하지 않는다.
void Dispatcher::drawMultiThreaded() {
	if ( drawEvents_.empty() ) {
		return;
	}

	// DrawEvent는 jobSize 단위로 스레드들에 분배될 것이므로,
	// 동기화를 위한 latch를 준비한다.
	// 이때, DrawEvent의 개수가 jobSize로 나누어 떨어지지 않을 경우를 대비한다.
	const std::size_t jobCnt = (drawEvents_.size() + (jobSizeDraw_ - 1)) / jobSizeDraw_;
	auto latch = std::latch( jobCnt );

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
	if ( allocatedCmdListCnt != requiredCmdListCnt ) {
		// 필요한 만큼 명령 컨텍스트가 할당되지 않았을 경우,
		// 명령 컨텍스트들을 사용하지 않고 그대로 반납하며,
		// 함수도 그대로 반환한다.
		// 추후 이 경우에도 동작할 수 있도록 대응하도록 한다..
		cmdListPool_->free( CommandListUsage::RenderingSlave, std::move( cmdCtxs ) );
		return;
	}

	// drawEvents의 [accEventCnt, accEventCnt + jobSizeDraw_) 범위의
	// 데이터를 가공해 perDrawcallData를 구축하고, 드로우콜을 수행한다.
	std::size_t accEventCnt = 0u;
	// 각 작업마다 명령 컨텍스트를 분배한다.
	auto currCmdCtx = cmdCtxs.begin();

	// 정렬된 이벤트에서 첫 additive 이벤트 인덱스를 파악한다.
	// (각 job이 PSO 전환 여부를 판단하는 데 사용)
	const std::size_t additiveStart = static_cast<std::size_t>(
		std::ranges::lower_bound( drawEvents_, false, {},
			[]( const DrawEvent& e ) { return !e.additive; }
		) - drawEvents_.begin()
	);

	while ( accEventCnt + (jobSizeDraw_ - 1) < drawEvents_.size() ) {
		// 명령 컨텍스트 초기화
		auto hrCmdAllocReset = currCmdCtx->cmdAlloc->Reset();
		DISPLAY_ERROR_DX_HR( hrCmdAllocReset, false );
		if ( hrCmdAllocReset < 0 ) {
			cmdListPool_->free( CommandListUsage::RenderingSlave, std::move( cmdCtxs ) );
			return;
		}
		auto hrCmdListReset = currCmdCtx->cmdList->Reset( currCmdCtx->cmdAlloc.Get(), nullptr );
		DISPLAY_ERROR_DX_HR( hrCmdListReset, false );
		if ( hrCmdListReset < 0 ) {
			cmdListPool_->free( CommandListUsage::RenderingSlave, std::move( cmdCtxs ) );
			return;
		}

		// 이 job의 첫 이벤트가 additive 구간에 속하는지 여부로 초기 PSO 결정
		auto* initialPso = (accEventCnt >= additiveStart) ? shaderAdditive_.Get() : shader_.Get();

		// 명령 기록
		addJobDraw( currCmdCtx->cmdList.Get(), drawEvents_.data() + accEventCnt,
			drawEvents_.data() + accEventCnt + jobSizeDraw_, accEventCnt, latch,
			initialPso
		);

		accEventCnt += jobSizeDraw_;
		++currCmdCtx;
	}

	// 찌꺼기 처리
	if ( accEventCnt != drawEvents_.size() ) {
		const auto lastJobSize = drawEvents_.size() - accEventCnt;
		// 명령 컨텍스트 초기화
		auto hrCmdAllocReset = currCmdCtx->cmdAlloc->Reset();
		DISPLAY_ERROR_DX_HR( hrCmdAllocReset, false );
		if ( hrCmdAllocReset < 0 ) {
			cmdListPool_->free( CommandListUsage::RenderingSlave, std::move( cmdCtxs ) );
			return;
		}
		auto hrCmdListReset = currCmdCtx->cmdList->Reset( currCmdCtx->cmdAlloc.Get(), nullptr );
		DISPLAY_ERROR_DX_HR( hrCmdListReset, false );
		if ( hrCmdListReset < 0 ) {
			cmdListPool_->free( CommandListUsage::RenderingSlave, std::move( cmdCtxs ) );
			return;
		}

		auto* initialPso = (accEventCnt >= additiveStart) ? shaderAdditive_.Get() : shader_.Get();

		// 명령 기록
		addJobDraw( currCmdCtx->cmdList.Get(), drawEvents_.data() + accEventCnt,
			drawEvents_.data() + accEventCnt + lastJobSize, accEventCnt, latch,
			initialPso
		);
	}

	// 동기화
	latch.wait();

	// 명령 기록 끝, 실행
	auto stagedCmdLists = std::vector<ID3D12CommandList*>( cmdCtxs.size(), nullptr );
	std::ranges::transform( cmdCtxs, stagedCmdLists.begin(),
		[]( const CommandContext& cmdCtx ) { return cmdCtx.cmdList.Get(); }
	);

	DISPLAY_ERROR_DX_VOID( cmdQ_->ExecuteCommandLists(
		static_cast<UINT>(stagedCmdLists.size()), stagedCmdLists.data()
	), false );

	// Fence 객체에 사용한 명령 컨텍스트들을 연관시켜 놓는다.
	pFence_->associatedCmdCtxs_[etoi( CommandListUsage::RenderingSlave )]
		.splice( pFence_->associatedCmdCtxs_[etoi( CommandListUsage::RenderingSlave )].end(), std::move( cmdCtxs ) );
}

// 멀티스레드 작업 시, GPU 데이터 갱신 작업에 대해
// 단위 작업을 생성하여 스레드에 할당하는데 사용된다.
void MU_CALLCONV Dispatcher::addJobUpdate( mu::Mat4x4 viewProj, const DrawEvent* pFirst,
	const DrawEvent* pLast, BillboardShader::PerInstanceData* pOut, std::latch& latch
) {
	threadPool_->addJob( [=, &latch]() {
		std::transform( pFirst, pLast, pOut,
			[viewProj]( const BillboardPipeline::DrawEvent& drawEvent ) {
				return BillboardShader::PerInstanceData{
					.world = mu::transpose( drawEvent.world ).getXmf(),
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
	std::size_t firstInstanceOffset, std::latch& latch,
	ID3D12PipelineState* pso
) {
	const auto jobSize = pLast - pFirst;

	threadPool_->addJob( [=, &latch]() {
		// 명령 컨텍스트마다 개별적으로 파이프라인 설정을 해주어야 한다.
		// (파이프라인 설정은 공유되지 않는다. 그렇더라.)
		DISPLAY_ERROR_DX_VOID( threadCmdList->SetGraphicsRootSignature( rootSig_->get() ), false );
		DISPLAY_ERROR_DX_VOID( threadCmdList->SetPipelineState( pso ), false );
		DISPLAY_ERROR_DX_VOID( threadCmdList->OMSetRenderTargets( 1u, &rtv_, false, &dsv_ ), false );
		DISPLAY_ERROR_DX_VOID( threadCmdList->RSSetViewports( 1u, &viewport_ ), false );
		DISPLAY_ERROR_DX_VOID( threadCmdList->RSSetScissorRects( 1u, &scissorRect_ ), false );

		// bindless 환경 세팅
		// d3d12단 Descriptor Heap, Descriptor Table 설정
		auto descriptorHeapsRaw = std::vector<ID3D12DescriptorHeap*>( descriptorHeaps_.size() );
		std::ranges::transform( descriptorHeaps_, descriptorHeapsRaw.begin(),
			[]( ComPtr<ID3D12DescriptorHeap>& comPtrHeap ) { return comPtrHeap.Get(); }
		);
		DISPLAY_ERROR_DX_VOID( threadCmdList->SetDescriptorHeaps(
			static_cast<UINT>(descriptorHeapsRaw.size()), descriptorHeapsRaw.data()
		), false );

		pTexPool_->bind( threadCmdList, rootParamIdxTexPool_ );
		pTexArrayPool_->bind( threadCmdList, rootParamIdxTexArrayPool_ );
		pTexCubePool_->bind( threadCmdList, rootParamIdxTexCubePool_ );
		pSamPool_->bind( threadCmdList, rootParamIdxSamPool_ );
		pCmpSamPool_->bind( threadCmdList, rootParamIdxCmpSamPool_ );

		DISPLAY_ERROR_DX_VOID( threadCmdList->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_POINTLIST ), false );

		// 바인드해야 하는 GPU 데이터는 다음 두 종류다. (셰이더 참고)
		// - PerInstanceData
		// - PerDrawcallData
		// - PerFrameData

		// PerInstanceData 바인드
		pResources_->perInstanceData.bind( threadCmdList, rootParamIdxPID_, roomIdx_ );
		// PerFrameData 바인드
		pResources_->perFrameData.bind( threadCmdList, rootParamIdxPFD_, roomIdx_ );

		bool jobCurrentAdditive = pFirst->additive;

		for ( auto idxDrawcall = firstInstanceOffset;
			idxDrawcall < firstInstanceOffset + jobSize;
			++idxDrawcall
			) {
			// DrawEvent의 정보를 기반으로 GPU 데이터 업데이트 및
			// 입력 조립기 설정을 하고 드로우콜을 수행한다.
			const auto& drawEvent = drawEvents_[idxDrawcall];

			// job이 additive 경계를 걸칠 경우 PSO 전환
			if ( drawEvent.additive != jobCurrentAdditive ) {
				jobCurrentAdditive = drawEvent.additive;
				auto* nextPso = jobCurrentAdditive ? shaderAdditive_.Get() : shader_.Get();
				DISPLAY_ERROR_DX_VOID( threadCmdList->SetPipelineState( nextPso ), false );
			}

			// PerDrawcallData 바인드
			pResources_->perDrawcallData.cbuffers[idxDrawcall].bind(
				threadCmdList, rootParamIdxPDD_, roomIdx_
			);

			auto perDrawcallData = BillboardShader::PerDrawcallData{
				.material = BillboardShader::Material{
					.idxTex = drawEvent.pTex->idxSrv,
					.tint = drawEvent.tint.getXmf()
				},
				.firstInstanceOffset = static_cast<u32t>(idxDrawcall)
			};
			// PerDrawcallData GPU 데이터 갱신
			// (바인드와 GPU 데이터 갱신 순서는 상관없다.
			//  어차피 바인드는 GPU 명령이라 바로 실행되지 않기 때문에)
			pResources_->perDrawcallData.cbuffers[idxDrawcall].stage(
				roomIdx_, &perDrawcallData, 1u
			);

			DISPLAY_ERROR_DX_VOID(
				threadCmdList->IASetVertexBuffers( 0u, static_cast<UINT>(Detail::staticVBViewPoints.size()),
					Detail::staticVBViewPoints.data()
				), false
			);
			DISPLAY_ERROR_DX_VOID( threadCmdList->IASetIndexBuffer( &Detail::staticIBViewPoint ), false );

			DISPLAY_ERROR_DX_VOID( threadCmdList->DrawIndexedInstanced(
				static_cast<UINT>(Detail::staticIBViewPoint.SizeInBytes / sizeof( u16t )),
				1u, 0u, 0, 0u
			), false );
		}

		// 명령 기록 종료
		DISPLAY_ERROR_DX_HR( threadCmdList->Close(), false );
		latch.count_down();
	});
}

}	// namespace BillboardPipeline