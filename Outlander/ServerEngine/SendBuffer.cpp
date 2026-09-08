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
	// release() 이후엔 정적 큐 재활용이 위험하다(프로세스 종료 중 큐가 파괴되는 중일 수 있음).
	// 재enqueue 대신 청크를 실제로 해제한다.
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

	// 호출 스레드(메인)의 TLS 청크부터 비운다. deleter가 odelete로 실제 해제한다.
	LSendBufferChunk.reset();

	// 종료된 스레드들이 반납해 둔 청크를 drain한다. 각 reset()이 deleter를 타며,
	// shuttingDown_이 켜져 있으므로 재enqueue 대신 실제 해제된다.
	std::shared_ptr<SendBufferChunk> chunk;
	while (sendBufferChunks_.try_dequeue(chunk)) {
		chunk.reset();
	}
}

ccqueue< std::shared_ptr<SendBufferChunk> > SendBufferManager::sendBufferChunks_{};
std::atomic<bool> SendBufferManager::shuttingDown_{false};
