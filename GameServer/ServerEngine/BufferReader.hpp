#ifndef buffer_reader_hpp
#define buffer_reader_hpp

#include "types.hpp"

class BufferReader {
public:
	BufferReader(byte* buffer, uint32 size)
		: buffer_(buffer), size_(size), readPos_(0u) {}

	bool read(void* dest, uint32 len) {
		if (freeSize() < len) {
			return false;
		}
		
		memcpy(dest, &buffer_[readPos_], len);
		readPos_ += len;
		return true;
	}

	uint32 readSize() const { return readPos_; }
	uint32 freeSize() const { return size_ - readPos_; }

	template<class T>
	BufferReader& operator>>(T& dest) {
		dest = *reinterpret_cast<T*>(&buffer_[readPos_]);
		readPos_ += sizeof(T);
		return *this;
	}

private:
	byte* buffer_;
	uint32 size_;
	uint32 readPos_;
};

#endif // buffer_reader_hpp