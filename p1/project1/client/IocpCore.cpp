#include "pch.hpp"
#include "global.hpp"
#include "IoEvent.hpp"
#include "IocpCore.hpp"
#include "standalone/game.hpp"

bool IocpCore::dispatch( uint32 timeoutMs ) {
	DWORD numBytes{ };
	ULONG_PTR key{ };
	IoEvent* event = nullptr;

	if ( ::GetQueuedCompletionStatus( iocpHandle_, &numBytes, &key,
		reinterpret_cast<LPOVERLAPPED*>( &event ), timeoutMs )
		) {
		auto iocpObject = event->getOwner( );
		iocpObject->dispatch( event, numBytes );
	}
	else {
		const int32 errCode = ::WSAGetLastError( );
		switch ( errCode ) {
		case WAIT_TIMEOUT:
			return false;

		case ERROR_CONNECTION_REFUSED:	// 서버가 오프라인 상태라면 standalone 모드로 전환
			std::cout << "[IocpCore] GetQueuedCompletionStatus failed with error (error code - " << errCode << ") : "
				<< std::system_category( ).message( errCode ) << '\n';

			pGame = std::make_unique<StandAlone::Game>( );
			static_cast<StandAlone::Game*>( pGame.get( ) )->setupStage( );
			gReady.store( true );

			return false;

		default:
			std::cout << "[IocpCore] GetQueuedCompletionStatus failed with error (error code - " << errCode << ") : "
				<< std::system_category( ).message( errCode ) << '\n';
			return false;
		}
	}

	return true;
}