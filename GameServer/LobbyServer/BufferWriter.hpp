#ifndef buffer_writer_hpp
#define buffer_writer_hpp

#include <cstdint>
#include <memory>
#include <utility>

class BufferWriter {
public:
	BufferWriter(std::uint8_t* buffer, std::uint32_t size)
		: buffer_(buffer), size_(size), writePos_(0u) {
	}

	~BufferWriter() = default;

	bool write(void* src, std::uint32_t len) {
		if (freeSize() < len) {
			return false;
		}

		::memcpy(&buffer_[writePos_], src, len);
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

	std::uint32_t writeSize() const { return writePos_; }
	std::uint32_t freeSize() const { return size_ - writePos_; }

	template<class T>
	BufferWriter& operator<<(T&& src) {
		using DataType = std::remove_reference_t<T>;
		*reinterpret_cast<DataType*>(&buffer_[writePos_]) = std::forward<DataType>(src);
		writePos_ += sizeof(T);
		return *this;
	}

private:
	std::uint8_t* buffer_;
	std::uint32_t size_;
	std::uint32_t writePos_;
};

#endif	// buffer_writer_hpp

