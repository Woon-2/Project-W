#ifndef buffer_writer_hpp
#define buffer_writer_hpp

#include "types.hpp"

class BufferWriter {
public:
	BufferWriter(byte* buffer, uint32 size)
		: buffer_(buffer), size_(size), writePos_(0u) {}

	bool write(void* src, uint32 len) {
		if (freeSize() < len) {
			return false;
		}

		memcpy(&buffer_[writePos_], src, len);
		writePos_ += len;
		return true;
	}

	template<class T>
	T* reserve() {
		if (freeSize() < sizeof(T)) {
			return nullptr;
		}

		T* ptr = reinterpret_cast<T*>(&buffer_[writePos_]);
		writePos_ += sizeof(T);
		return ptr;
	}

	uint32 writeSize() const { return writePos_; }
	uint32 freeSize() const { return size_ - writePos_; }

	template<class T>
	BufferWriter& operator<<(T&& src) {
		using DataType = std::remove_reference_t<T>;
		*reinterpret_cast<DataType*>(&buffer_[writePos_]) = std::forward<DataType>(src);
		writePos_ += sizeof(T);
		return *this;
	}

private:
	byte* buffer_;
	uint32 size_;
	uint32 writePos_;
};

#endif // buffer_writer_hpp