#include "sepch.hpp"
#include "SendBuffer.hpp"

/*------------------
     SendBuffer
------------------*/

void SendBuffer::close(uint32 writeSize) {
    ASSERT_CRASH(writeSize <= allocSize_);
	writeSize_ = writeSize;
	owner_->close(writeSize_);
}

/*-------------------------
     SendBufferManager
-------------------------*/

std::shared_ptr<SendBuffer> SendBufferManager::open( uint32 size ) {
	if ( LSendBufferChunk == nullptr ) {
		LSendBufferChunk = pop();
		LSendBufferChunk->reset();
	}

	ASSERT_CRASH( LSendBufferChunk->isOpen() == false );

	// 다 썼으면 새로운 chunk을 가져온다.
	if (size > LSendBufferChunk->freeSize()) {
		LSendBufferChunk = pop();
		LSendBufferChunk->reset();
	}

	return LSendBufferChunk->open( size );
}

void SendBufferManager::push( SendBufferChunk* chunk ) {
	// After release(), recycling into the static queue is unsafe (queue may be
	// mid-destruction during process exit) - free the chunk for real instead.
	if (shuttingDown_.load(std::memory_order_relaxed)) {
		odelete(chunk);
		return;
	}

	sendBufferChunks_.enqueue( std::shared_ptr<SendBufferChunk>( chunk, push ) );
}

std::shared_ptr<SendBufferChunk> SendBufferManager::pop() {
	std::shared_ptr<SendBufferChunk> chunk;

	if (sendBufferChunks_.try_dequeue(chunk)) {
		return chunk;
	}

	return std::shared_ptr<SendBufferChunk>( onew<SendBufferChunk>(), push );
}

void SendBufferManager::release() {
	shuttingDown_.store(true, std::memory_order_relaxed);

	// Drop the calling thread's TLS chunk first; its deleter now frees via odelete.
	LSendBufferChunk.reset();

	// Drain chunks recycled by exited threads. Each reset() triggers the deleter,
	// which frees the chunk instead of re-enqueueing (shuttingDown_ is set).
	std::shared_ptr<SendBufferChunk> chunk;
	while (sendBufferChunks_.try_dequeue(chunk)) {
		chunk.reset();
	}
}

ccqueue< std::shared_ptr<SendBufferChunk> > SendBufferManager::sendBufferChunks_{};
std::atomic<bool> SendBufferManager::shuttingDown_{false};
