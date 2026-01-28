#ifndef send_buffer_hpp
#define send_buffer_hpp

#include "Memory.hpp"

/*------------------
     SendBuffer
------------------*/

class SendBufferChunk;
class SendBuffer : std::enable_shared_from_this<SendBuffer> {
public:
	SendBuffer(uint8* buffer, uint32 allocSize, SendBufferChunk* owner)
		: buffer_(buffer), allocSize_(allocSize), writeSize_(0), owner_(owner) {}

	uint8* data() { return buffer_; }
	int32 size() const { return writeSize_; }

private:
	uint8* buffer_;
	uint32 allocSize_;
	uint32 writeSize_;
	SendBufferChunk* owner_;
};

/*-----------------------
	 SendBufferChunk
-----------------------*/

class SendBufferChunk {
public:
	SendBufferChunk() : buffer_(), open_(false), usedSize_(0) {}

	SendBuffer* open(uint32 size) {
		ASSERT_CRASH(size <= chunkSize_);
		ASSERT_CRASH(!open_);

		if (size > freeSize()) {
			return nullptr;
		}

		open_ = true;
		return xnew<SendBuffer>(size);
	}

	void close(uint32 usedSize) {
		ASSERT_CRASH(open_);
		open_ = false;
		usedSize_ += usedSize;
	}

	void reset() {
		open_ = false;
		usedSize_ = 0;
	}

	uint8* buffer() { return &buffer_[usedSize_]; }
	bool isOpen() const { return open_; }
	uint32 freeSize() const { return chunkSize_ - usedSize_; }

private:
	static const int32 chunkSize_{0x1000};

	std::array<uint8, chunkSize_> buffer_;
	bool open_;
	uint32 usedSize_;
};

/*-------------------------
	 SendBufferManager
-------------------------*/

class SendBufferManager {
public:
	static SendBuffer* open(uint32 size);

private:
	static SendBufferChunk* pop();
	static void push(SendBufferChunk* buffer);

private:
	static ccqueue<SendBufferChunk*> sendBufferChunks_;
};

#endif	// send_buffer_hpp