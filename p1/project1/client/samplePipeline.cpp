#include "samplePipeline.hpp"
#include "shader.hpp"
#include "mesh.hpp"
#include "errorHandling.hpp"

namespace SamplePipeline {

Dispatcher::Dispatcher( const std::shared_ptr<RootSig>& rootSig, const ComPtr<ID3D12PipelineState>& shader,
	const ComPtr<ID3D12CommandQueue>& cmdQ, const D3D12_VIEWPORT& viewport, const D3D12_RECT& scissorRect,
	D3D12_CPU_DESCRIPTOR_HANDLE rtv, D3D12_CPU_DESCRIPTOR_HANDLE dsv, Fence* pFence, Resources* pResources,
	ThreadPool* threadPool, CommandListPool* commandListPool, std::vector<DrawEvent>&& drawEvents, std::size_t roomIdx
) : rootSig_(rootSig), shader_(shader), cmdQ_(cmdQ), viewport_(viewport), scissorRect_(scissorRect),
	rtv_(rtv), dsv_(dsv), pFence_(pFence), pResources_(pResources), threadPool_(threadPool),
	cmdListPool_(commandListPool), drawEvents_(std::move(drawEvents)), roomIdx_(roomIdx),
	rootParamIdxPDD_(rootSig->paramIdx(L"PerDrawcallData")),
	rootParamIdxPID_(rootSig->paramIdx(L"PerInstanceData")) {}

void Dispatcher::updateGPUDataSingleThreaded() {
	// 메시 데이터 업로드
	// 정렬을 통해 인스턴싱이 가능하도록 한다.
	std::sort(drawEvents_.begin(), drawEvents_.end());
	
	// perInstanceData를 static으로 선언하여
	// 매번 처음부터 메모리를 구축하지 않고 재사용할 수 있도록 한다.
	static auto perInstanceData = std::vector<SampleShader::PerInstanceData>();
	perInstanceData.resize(drawEvents_.size());

	std::ranges::transform(drawEvents_, perInstanceData.begin(),
		[](const SamplePipeline::DrawEvent& drawEvent) {
			return SampleShader::PerInstanceData{
				.wvp = mu::transpose(drawEvent.world).getXmf()
			};
		}	
	);

	pResources_->perInstanceData.stage(roomIdx_, perInstanceData);
	perInstanceData.clear();
}

void Dispatcher::updateGPUDataMultiThreaded() {
	// 메시 데이터 업로드
	// 정렬을 통해 인스턴싱이 가능하도록 한다.
	std::sort(drawEvents_.begin(), drawEvents_.end());
	
	// perInstanceData를 static으로 선언하여
	// 매번 처음부터 메모리를 구축하지 않고 재사용할 수 있도록 한다.
	static auto perInstanceData = std::vector<SampleShader::PerInstanceData>();
	perInstanceData.resize(drawEvents_.size());

	auto latch = std::latch( drawEvents_.size() / jobSizeUpdate_
		+ ((drawEvents_.size() % jobSizeUpdate_) != 0)
	);

	std::size_t accEventCnt = 0u;
	while (accEventCnt + (jobSizeUpdate_ - 1) < drawEvents_.size()) {
		addJobUpdate( drawEvents_.data() + accEventCnt,
			drawEvents_.data() + accEventCnt + (jobSizeUpdate_ - 1),
			perInstanceData.data() + accEventCnt, latch
		);

		accEventCnt += jobSizeUpdate_;
	}

	if (accEventCnt != drawEvents_.size()) {
		const auto lastJobSize = drawEvents_.size() - accEventCnt;

		addJobUpdate( drawEvents_.data() + accEventCnt,
			drawEvents_.data() + accEventCnt + (lastJobSize - 1),
			perInstanceData.data() + accEventCnt, latch
		);
	}

	latch.wait();
	pResources_->perInstanceData.stage(roomIdx_, perInstanceData);
	perInstanceData.clear();
}

void Dispatcher::drawSingleThreaded() {
	CommandContext cmdCtx{};
	DISPLAY_ERROR_STR( cmdListPool_->allocOne(CommandListUsage::RenderingSlave, cmdCtx),
		L"[GFX Error] GFX::drawSingleThreaded: 요청한 명령 리스트를 할당받지 못했습니다.", false
	);
	if (!cmdCtx.cmdList) {
		return;
	}

	auto cmdList = cmdCtx.cmdList.Get();
	auto cmdAlloc = cmdCtx.cmdAlloc.Get();
	cmdAlloc->Reset();
	cmdList->Reset(cmdAlloc, nullptr);

	cmdList->SetGraphicsRootSignature(rootSig_->get());
	cmdList->SetPipelineState(shader_.Get());
	cmdList->OMSetRenderTargets(1u, &rtv_, false, &dsv_);
	cmdList->RSSetViewports(1u, &viewport_);
	cmdList->RSSetScissorRects(1u, &scissorRect_);

	DISPLAY_ERROR_DX_VOID( cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST), false );

	pResources_->perInstanceData.bind(cmdList, rootParamIdxPID_, roomIdx_);

	u32t idxDrawcall = 0u;

	for (const auto& drawEvent : drawEvents_) {
		pResources_->perDrawcallData.cbuffers[idxDrawcall].bind(
			cmdList, rootParamIdxPDD_, roomIdx_
		);

		auto perDrawcallData = SampleShader::PerDrawcallData{
			.firstInstanceIdx = idxDrawcall
		};
		pResources_->perDrawcallData.cbuffers[idxDrawcall].stage(
			roomIdx_, &perDrawcallData, 1u
		);

		DISPLAY_ERROR_DX_VOID(
			cmdList->IASetVertexBuffers(0u, static_cast<UINT>(drawEvent.mesh->vbViews.size()),
				drawEvent.mesh->vbViews.data()	
			), false
		);
		DISPLAY_ERROR_DX_VOID( cmdList->IASetIndexBuffer(&drawEvent.subMesh->ibView), false );

		DISPLAY_ERROR_DX_VOID( cmdList->DrawIndexedInstanced(
			static_cast<UINT>(drawEvent.subMesh->ibView.SizeInBytes / sizeof(u16t)),
			1u, 0u, 0, 0u
		), false );

		++idxDrawcall;
	}

	cmdList->Close();
	ID3D12CommandList* stagedCmdLists[] = {cmdList};

	cmdQ_->ExecuteCommandLists(1u, stagedCmdLists);

	pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)]
		.push_back(std::move(cmdCtx));
}

void Dispatcher::drawMultiThreaded() {
	std::size_t accEventCnt = 0u;

	auto latch = std::latch( drawEvents_.size() / jobSizeDraw_
		+ ((drawEvents_.size() % jobSizeDraw_) != 0)
	);

	std::list<CommandContext> cmdCtxs{};
	const auto requiredCmdListCnt = (drawEvents_.size() + (jobSizeDraw_ - 1)) / jobSizeDraw_;

	const auto allocatedCmdListCnt = cmdListPool_->alloc(
		requiredCmdListCnt, CommandListUsage::RenderingSlave, cmdCtxs
	);

	DISPLAY_ERROR_STR( allocatedCmdListCnt == requiredCmdListCnt,
		L"[GFX Error] GFX::renderSampleShaderDispatch: 요청한 수 만큼의 명령 리스트를 할당받지 못했습니다.",
		false
	);
	if (allocatedCmdListCnt != requiredCmdListCnt) {
		cmdListPool_->free(CommandListUsage::RenderingSlave, std::move(cmdCtxs));
		return;
	}

	auto currCmdCtx = cmdCtxs.begin();

	while (accEventCnt + (jobSizeDraw_ - 1) < drawEvents_.size()) {
		currCmdCtx->cmdAlloc->Reset();
		currCmdCtx->cmdList->Reset(currCmdCtx->cmdAlloc.Get(), nullptr);

		addJobDraw(currCmdCtx->cmdList.Get(), drawEvents_.data() + accEventCnt,
			drawEvents_.data() + accEventCnt + (jobSizeDraw_ - 1), accEventCnt, latch
		);
		
		accEventCnt += jobSizeDraw_;
		++currCmdCtx;
	}

	if (accEventCnt != drawEvents_.size()) {
		const auto lastJobSize = drawEvents_.size() - accEventCnt;

		addJobDraw(currCmdCtx->cmdList.Get(), drawEvents_.data() + accEventCnt,
			drawEvents_.data() + accEventCnt + (lastJobSize - 1), accEventCnt, latch
		);
	}

	latch.wait();

	auto stagedCmdLists = std::vector<ID3D12CommandList*>(cmdCtxs.size(), nullptr);
	std::ranges::transform(cmdCtxs, stagedCmdLists.begin(),
		[](const CommandContext& cmdCtx) { return cmdCtx.cmdList.Get(); }	
	);

	cmdQ_->ExecuteCommandLists(static_cast<UINT>(stagedCmdLists.size()), stagedCmdLists.data());

	pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)]
		.splice( pFence_->associatedCmdCtxs_[etoi(CommandListUsage::RenderingSlave)].end(), std::move(cmdCtxs) );
}

void Dispatcher::addJobUpdate( const DrawEvent* pFirst, const DrawEvent* pLast,
	SampleShader::PerInstanceData* pOut, std::latch& latch
) {
	threadPool_->addJob( [=, &latch](){
		std::transform( pFirst, pLast, pOut,
			[](const SamplePipeline::DrawEvent& drawEvent) {
				return SampleShader::PerInstanceData{
					.wvp = mu::transpose(drawEvent.world).getXmf()
				};
			}	
		);

		latch.count_down();
	});
}

void Dispatcher::addJobDraw( ID3D12GraphicsCommandList* threadCmdList,
	const DrawEvent* pFirst, const DrawEvent* pLast,
	std::size_t firstInstanceIdx, std::latch& latch
) {
	threadPool_->addJob([=, &latch]() {
		threadCmdList->SetGraphicsRootSignature(rootSig_->get());
		threadCmdList->SetPipelineState(shader_.Get());
		threadCmdList->OMSetRenderTargets(1u, &rtv_, false, &dsv_);
		threadCmdList->RSSetViewports(1u, &viewport_);
		threadCmdList->RSSetScissorRects(1u, &scissorRect_);
				
		DISPLAY_ERROR_DX_VOID( threadCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST), false );

		pResources_->perInstanceData.bind(threadCmdList, rootParamIdxPID_, roomIdx_);

		for ( auto idxDrawcall = firstInstanceIdx;
			idxDrawcall < firstInstanceIdx + jobSizeDraw_;
			++idxDrawcall
		) {
			pResources_->perDrawcallData.cbuffers[idxDrawcall].bind(
				threadCmdList, rootParamIdxPDD_, roomIdx_
			);

			auto perDrawcallData = SampleShader::PerDrawcallData{
				.firstInstanceIdx = static_cast<u32t>(idxDrawcall)
			};
			pResources_->perDrawcallData.cbuffers[idxDrawcall].stage(
				roomIdx_, &perDrawcallData, 1u
			);

			const auto& drawEvent = drawEvents_[idxDrawcall];

			DISPLAY_ERROR_DX_VOID(
				threadCmdList->IASetVertexBuffers(0u, static_cast<UINT>(drawEvent.mesh->vbViews.size()),
					drawEvent.mesh->vbViews.data()	
				), false
			);
			DISPLAY_ERROR_DX_VOID( threadCmdList->IASetIndexBuffer(&drawEvent.subMesh->ibView), false );

			DISPLAY_ERROR_DX_VOID( threadCmdList->DrawIndexedInstanced(
				static_cast<UINT>(drawEvent.subMesh->ibView.SizeInBytes / sizeof(u16t)),
				1u, 0u, 0, 0u
			), false );
		}

		threadCmdList->Close();
		latch.count_down();
	} );
}

}	// namespace SamplePipeline