#ifndef SEND_BUFFER_HPP
#define SEND_BUFFER_HPP

class SendBuffer : public std::enable_shared_from_this<SendBuffer> {
public:
	SendBuffer( int32 bufferSize ) : writeSize_( 0 ), buffer_( bufferSize ) {}

	uint8* data( ){
		return buffer_.data( );
	}

	int32 writeSize( ) const {
		return writeSize_;
	}

	int32 capacity( ) const {
		return static_cast<int32>( buffer_.size( ) );
	}

	void copyData( void* data, int32 len ) {
		ASSERT_CRASH( len <= capacity( ) );
		::memcpy( buffer_.data( ), data, len );
		writeSize_ = len;
	}

private:
	int32 writeSize_;
	std::vector<uint8> buffer_;
};

#endif // SEND_BUFFER_HPP