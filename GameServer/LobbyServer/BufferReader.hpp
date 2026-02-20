#ifndef buffer_reader_hpp
#define buffer_reader_hpp

#include <cstdint>
#include <memory>

class BufferReader {
public:
	BufferReader(std::uint8_t* buffer, std::uint32_t size)
		: buffer_(buffer), size_(size), readPos_(0u) {}

	~BufferReader() = default;

	bool read(void* dest, std::uint32_t len) {
		if (freeSize() < len) {
			return false;
		}

		::memcpy(dest, &buffer_[readPos_], len);
		readPos_ += len;
		return true;
	}

	std::uint32_t readSize() const { return readPos_; }
	std::uint32_t freeSize() const { return size_ - readPos_; }

	template<class T>
	BufferReader& operator>>(T& dest) {
		dest = *reinterpret_cast<T*>(&buffer_[readPos_]);
		readPos_ += sizeof(T);
		return *this;
	}

private:
	std::uint8_t* buffer_;
	std::uint32_t size_;
	std::uint32_t readPos_;
};

#endif	// buffer_reader_hpp