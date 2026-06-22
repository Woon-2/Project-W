#include "pch.hpp"
#include "renderSubmitter.hpp"
#include "errorHandling.hpp"

RenderSubmitter::~RenderSubmitter() {
	stop();
}

void RenderSubmitter::start(ID3D12CommandQueue* cmdQ, bool async) {
	cmdQ_ = cmdQ;
	async_ = async;
	done_ = false;
	if (async_) {
		thread_ = std::thread([this]() { this->worker(); });
	}
}

void RenderSubmitter::goAsync() {
	if (async_ || thread_.joinable()) {
		return;
	}
	async_ = true;
	done_ = false;
	thread_ = std::thread([this]() { this->worker(); });
}

void RenderSubmitter::stop() {
	if (thread_.joinable()) {
		{
			std::lock_guard<std::mutex> lk(mtx_);
			done_ = true;
		}
		cv_.notify_all();
		thread_.join();
	}
	cmdQ_ = nullptr;
}

void RenderSubmitter::submit(unsigned count, ID3D12CommandList* const* lists) {
	Item item{};
	item.kind = Kind::Execute;
	item.lists.assign(lists, lists + count);
	enqueue(std::move(item));
}

void RenderSubmitter::present(IDXGISwapChain* swapChain, unsigned syncInterval) {
	Item item{};
	item.kind = Kind::Present;
	item.swapChain = swapChain;
	item.syncInterval = syncInterval;
	enqueue(std::move(item));
}

void RenderSubmitter::signal(ID3D12Fence* fence, std::uint64_t value) {
	Item item{};
	item.kind = Kind::Signal;
	item.fence = fence;
	item.fenceValue = value;
	enqueue(std::move(item));
}

void RenderSubmitter::flushBlocking() {
	if (!async_) {
		return;	// inline mode: nothing is ever queued
	}
	std::unique_lock<std::mutex> lk(mtx_);
	drained_.wait(lk, [this]() { return inFlight_ == 0u; });
}

void RenderSubmitter::enqueue(Item&& item) {
	if (!async_) {
		run(item);	// inline: execute on the calling (main) thread, preserves order
		return;
	}
	{
		std::lock_guard<std::mutex> lk(mtx_);
		queue_.push_back(std::move(item));
		++inFlight_;
	}
	cv_.notify_one();
}

void RenderSubmitter::run(Item& item) {
	switch (item.kind) {
	case Kind::Execute:
		if (!item.lists.empty()) {
			DISPLAY_ERROR_DX_VOID(
				cmdQ_->ExecuteCommandLists(
					static_cast<UINT>(item.lists.size()), item.lists.data()
				), false
			);
		}
		break;
	case Kind::Present:
		DISPLAY_ERROR_DX_VOID(
			item.swapChain->Present(item.syncInterval, 0u), false
		);
		break;
	case Kind::Signal:
		DISPLAY_ERROR_DX_VOID(
			cmdQ_->Signal(item.fence, item.fenceValue), false
		);
		break;
	}
}

void RenderSubmitter::worker() {
	for (;;) {
		Item item{};
		{
			std::unique_lock<std::mutex> lk(mtx_);
			cv_.wait(lk, [this]() { return done_ || !queue_.empty(); });
			if (queue_.empty()) {
				// Only reached when done_ is set and the queue is fully drained.
				break;
			}
			item = std::move(queue_.front());
			queue_.pop_front();
		}

		run(item);

		{
			std::lock_guard<std::mutex> lk(mtx_);
			--inFlight_;
			if (inFlight_ == 0u && queue_.empty()) {
				drained_.notify_all();
			}
		}
	}
}
