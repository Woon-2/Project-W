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
	sendBufferChunks_.enqueue( std::shared_ptr<SendBufferChunk>( chunk, push ) );
}

std::shared_ptr<SendBufferChunk> SendBufferManager::pop() {
	std::shared_ptr<SendBufferChunk> chunk;

	if (sendBufferChunks_.try_dequeue(chunk)) {
		return chunk;
	}

	return std::shared_ptr<SendBufferChunk>( onew<SendBufferChunk>(), push );
}

ccqueue< std::shared_ptr<SendBufferChunk> > SendBufferManager::sendBufferChunks_{};
