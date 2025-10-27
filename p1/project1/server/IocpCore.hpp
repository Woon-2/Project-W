#ifndef IOCP_CORE_HPP
#define IOCP_CORE_HPP

#include "pch.hpp"
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

	bool registerHandle( IocpObject& iocpObject ) {
		return ::CreateIoCompletionPort( iocpObject.getHandle( ), iocpHandle_, 0, 0 );
	}

	bool dispatch( uint32 timeoutMs = INFINITE ) {
		DWORD numBytes{ };
		ULONG_PTR key{ };
		IoEvent* event = nullptr;

		if ( ::GetQueuedCompletionStatus( iocpHandle_, &numBytes, &key,
			reinterpret_cast<LPOVERLAPPED*>( &event ), timeoutMs )
		) {

		}
		else {
			const int32 errCode = ::WSAGetLastError( );
			switch ( errCode ) {
			case WAIT_TIMEOUT:
				return false;

			default:
				break;
			}
		}
	}

	HANDLE getHandle( ) const {
		return iocpHandle_;
	}

private:
	HANDLE iocpHandle_;
};

#endif // IOCP_CORE_HPP