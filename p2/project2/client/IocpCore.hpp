#ifndef IOCP_CORE_HPP
#define IOCP_CORE_HPP

#include "IocpObject.hpp"

class IocpCore {
public:
	IocpCore( ) 
		: iocpHandle_( ::CreateIoCompletionPort( INVALID_HANDLE_VALUE, nullptr, 0, 0 ) ) {
		ASSERT_CRASH( iocpHandle_ != nullptr );
	}

	~IocpCore( ) {
		::CloseHandle( iocpHandle_ );
	}

	bool registerHandle( const SPIocpObject& iocpObject ) {
		return ::CreateIoCompletionPort( iocpObject->getHandle( ), iocpHandle_, 0, 0 );
	}
	bool dispatch( uint32 timeoutMs = INFINITE );

	HANDLE getHandle( ) const { return iocpHandle_;	}

private:
	HANDLE iocpHandle_;
};

#endif // IOCP_CORE_HPP