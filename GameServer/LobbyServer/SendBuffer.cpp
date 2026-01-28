#include "pch.hpp"
#include "SendBuffer.hpp"

/*-------------------------
	 SendBufferManager
-------------------------*/

SendBuffer* SendBufferManager::open(uint32 size) {
	if (LSendBufferChunk == nullptr) {
		LSendBufferChunk = pop();
		LSendBufferChunk->reset();
	}

	ASSERT_CRASH(!LSendBufferChunk->isOpen());

	if (LSendBufferChunk->freeSize() < size) {
		push(LSendBufferChunk);
		LSendBufferChunk = pop();
		LSendBufferChunk->reset();
	}

	std::cout << "Free Size: " << LSendBufferChunk->freeSize() << '\n';

	return LSendBufferChunk->open(size);
}

SendBufferChunk* SendBufferManager::pop() {
	SendBufferChunk* chunk = nullptr;
	if (sendBufferChunks_.try_dequeue(chunk)) {
		return chunk;
	}

	return xnew<SendBufferChunk>();
}

void SendBufferManager::push(SendBufferChunk* buffer) {
	ASSERT_CRASH(sendBufferChunks_.enqueue(buffer));
}

ccqueue<SendBufferChunk*> SendBufferManager::sendBufferChunks_;
