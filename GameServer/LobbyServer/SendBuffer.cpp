#include "pch.hpp"
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

SendBuffer* SendBufferManager::open(uint32 size) {
	if (LSendBufferChunk == nullptr) {
		LSendBufferChunk = xnew<SendBufferChunk>();
	}
	else if (LSendBufferChunk->freeSize() < size) {
		LSendBufferChunk->reset();
	}

	//std::cout << "Free Size: " << LSendBufferChunk->freeSize() << '\n';

	return LSendBufferChunk->open(size);
}

void SendBufferManager::clear() {
	if (LSendBufferChunk != nullptr) {
		xdelete(LSendBufferChunk);
		LSendBufferChunk = nullptr;
	}
}
