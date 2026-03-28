#ifndef recv_buffer_hpp
#define recv_buffer_hpp

#include <vector>
#include "types.hpp"

/*
* 사용자는 bufferSize 크기의 버퍼를 갖는다고 생각하지만,
* 실제로는 bufferSize * bufferCount 크기의 버퍼를 갖는다.
* [     ] 가 bufferSize 크기의 버퍼라면
* [     ][     ][     ][     ][     ][     ][     ][     ][     ][     ] <- 실제 버퍼
* [                                                                    ] <- 정확히는 이런 모양의 버퍼이다.
* clean 멤버 함수의 첫 번째 if문에서 처리될 확률을 높이기 위해 버퍼 크기를 크게 잡음
*/

class RecvBuffer {
public:
	RecvBuffer(int32 bufferSize)
		: capacity_(bufferSize * bufferCnt_), bufferSize_(bufferSize),
		readPos_(0), writePos_(0), buffer_(capacity_) {}

	bool moveReadPos(int32 numBytes) {
		if (numBytes > dataSize()) {
			return false;
		}

		readPos_ += numBytes;
		return true;
	}

	bool moveWritePos(int32 numBytes) {
		if (numBytes > freeSize()) {
			return false;
		}

		writePos_ += numBytes;
		return true;
	}

	void clean() {
		if (dataSize() == 0) {
			// read, write 커서가 동일안 위치라면, 둘 다 초기화
			readPos_ = 0;
			writePos_ = 0;
		}
		else {
			// 여유 공간이 버퍼 1개 크기 미만일 경우, 데이터를 앞으로 당김
			if (freeSize() < bufferSize_) {
				memcpy(&buffer_[0], &buffer_[readPos_], dataSize());
				readPos_ = 0;
				writePos_ = dataSize();
			}
		}
	}

	byte* readPos() { return &buffer_[readPos_]; }
	byte* writePos() { return &buffer_[writePos_]; }
	int32 dataSize() const { return writePos_ - readPos_; }
	int32 freeSize() const { return capacity_ - writePos_; }

private:
	static const int32 bufferCnt_{10};	// 내부적으로 존재하는 bufferSize_ 크기의 버퍼 개수
	int32 capacity_;					// 전체 버퍼 크기
	int32 bufferSize_;					// 버퍼 1개 크기
	int32 readPos_;
	int32 writePos_;
	std::vector<byte> buffer_;
};

#endif // recv_buffer_hpp