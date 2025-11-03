#ifndef RECV_BUFFER_HPP
#define RECV_BUFFER_HPP

#include "pch.hpp"

class RecvBuffer {
	enum {
		bufferCount = 10
	};

public:
	RecvBuffer( int32 bufferSize )
		: capacity_( bufferSize * bufferCount ), bufferSize_( bufferSize )
		, readPos_( 0 ), writePos_( 0 ), buffer_( capacity_ ) {}

	void clean( ) {
		if ( dataSize( ) == 0 ) {
			// 읽기, 쓰기 커서가 동일한 위치라면, 둘 다 초기화
			readPos_ = 0;
			writePos_ = 0;
		}
		else {
			// 여유 공간이 버퍼 1개 크기 미만일 경우, 데이터를 앞으로 당김
			if ( freeSize( ) < bufferSize_ ) {
				::memcpy( &buffer_[ 0 ], &buffer_[ readPos_ ], dataSize( ) );
				readPos_ = 0;
				writePos_ = dataSize( );
			}
		}
	}

	bool moveReadPos( int32 numBytes ) {
		if ( numBytes > dataSize( ) ) {
			return false;
		}

		readPos_ += numBytes;
		return true;
	}

	bool moveWritePos( int32 numBytes ) {
		if( numBytes > freeSize( ) ) {
			return false;
		}

		writePos_ += numBytes;
		return true;
	}

	uint8* readPos( ) {
		return &buffer_[ readPos_ ];
	}

	uint8* writePos( ) {
		return &buffer_[ writePos_ ];
	}

	int32 dataSize( ) const {
		return writePos_ - readPos_;
	}

	int32 freeSize( ) const {
		return capacity_ - writePos_;
	}

private:
	int32 capacity_;	// clean 함수의 첫 번째 if 문에 걸릴 확률을 늘리기 위해 버퍼 크기를 bufferSize_ 보다 크게 잡음
						// 실제 버퍼 크기
	int32 bufferSize_;	// 단일 버퍼에 쓸 수 있는 최대 크기
	int32 readPos_;
	int32 writePos_;
	std::vector<uint8> buffer_;
};

#endif // RECV_BUFFER_HPP