#ifndef __renderSubmitter_HPP
#define __renderSubmitter_HPP

// RenderSubmitter owns the single graphics command queue submission path.
//
// Rationale: ExecuteCommandLists / Present / Signal on a single ID3D12CommandQueue
// are serialized by the driver regardless of the calling thread, and they are the
// dominant CPU cost on the main render thread. Moving every submission onto one
// dedicated consumer thread takes that cost off the main thread's critical path,
// so ECL of pass N overlaps with recording of pass N+1 on the main/worker threads.
//
// Invariants (see plan / graphicsArchitecture.md):
//  - The command queue is touched ONLY by the submission thread.
//  - The internal queue is a single-consumer FIFO: enqueue order == GPU submit order,
//    which preserves the existing inter-pass / barrier ordering.
//  - A command list must already be Close()'d before it is handed to submit().
//  - Command list object lifetime (ComPtr) is guaranteed elsewhere (the frame Fence's
//    associatedCmdCtxs_); submit() only stores raw ID3D12CommandList* pointers.
//  - Fence desiredValue ownership stays on the main thread; the submitter only calls
//    Signal(fence, value) with the value it is given.

#include <cstdint>
#include <deque>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

// COM interfaces are referenced by pointer only in this header.
struct ID3D12CommandQueue;
struct ID3D12CommandList;
struct ID3D12Fence;
struct IDXGISwapChain;

class RenderSubmitter {
public:
	RenderSubmitter() = default;
	~RenderSubmitter();

	RenderSubmitter(const RenderSubmitter&) = delete;
	RenderSubmitter& operator=(const RenderSubmitter&) = delete;

	// Binds the queue. The queue is owned by GFX; this class only borrows it.
	// async == false runs every call inline on the caller thread (used during
	// one-shot init-time GPU work, before the render loop, and to isolate regressions).
	void start(ID3D12CommandQueue* cmdQ, bool async = true);
	// Switches from inline to threaded mode by spawning the submission thread.
	// Idempotent; call once the render loop begins so init-time work stays inline.
	void goAsync();
	// Drains the queue and stops the submission thread. Safe to call multiple times.
	void stop();

	// Enqueues an ExecuteCommandLists. Argument order mirrors
	// ID3D12CommandQueue::ExecuteCommandLists(count, lists) so call sites stay identical.
	// The pointer array is copied into the item; the underlying lists must outlive
	// GPU execution (guaranteed by the frame Fence).
	void submit(unsigned count, ID3D12CommandList* const* lists);
	// Enqueues a swap-chain Present (must follow the present transition command list).
	void present(IDXGISwapChain* swapChain, unsigned syncInterval);
	// Enqueues a queue Signal(fence, value).
	void signal(ID3D12Fence* fence, std::uint64_t value);

	// Blocks until the submission queue is fully drained (no GPU wait).
	// Used on shutdown / resize / device reset before tearing the queue down.
	void flushBlocking();

private:
	enum class Kind { Execute, Present, Signal };

	struct Item {
		Kind kind;
		std::vector<ID3D12CommandList*> lists;	// Execute
		IDXGISwapChain* swapChain = nullptr;	// Present
		unsigned syncInterval = 0u;				// Present
		ID3D12Fence* fence = nullptr;			// Signal
		std::uint64_t fenceValue = 0u;			// Signal
	};

	void enqueue(Item&& item);
	void run(Item& item);	// performs the actual D3D12 call on the consumer thread
	void worker();

	ID3D12CommandQueue* cmdQ_ = nullptr;
	bool async_ = false;

	std::deque<Item> queue_{};
	std::mutex mtx_{};
	std::condition_variable cv_{};		// signaled when work is pushed
	std::condition_variable drained_{};	// signaled when the queue empties
	std::size_t inFlight_ = 0u;			// items pushed but not yet fully processed
	bool done_ = false;
	std::thread thread_{};
};

#endif	// __renderSubmitter_HPP
