#include "pch.hpp"
#include "global.hpp"  
#include "IocpCore.hpp"
#include "Listener.hpp"
#include "Service.hpp"
#include "Session.hpp"
#include "ServerSession.hpp"

/*---------------
     Service
---------------*/

SPSession Service::createSession( ) {
    auto session = factory_( );
	session->setService( shared_from_this( ) );

    if ( iocpCore_->registerHandle( session ) == false ) {
        return nullptr;
    }

    return session;
}

void Service::addSession( const SPSession& session ) {
	std::lock_guard<std::mutex> lock( mtx_ );
	sessions_.insert( session );
	++sessionCount_;
}

void Service::releaseSession( const SPSession& session ) {
    std::lock_guard<std::mutex> lock( mtx_ );
    ASSERT_CRASH( sessions_.erase( session ) != 0 );
	--sessionCount_;
}

/*----------------------
     Client Service
----------------------*/

bool ClientService::start( ) {
    if ( !canStart( ) ) {
        return false;
    }

    const auto sessionCount = getMaxSessionCount( );
    for ( auto i = 0; i < sessionCount; ++i ) {
		auto session = createSession( );

        if ( !session->connect( ) ) {
            return false;
        }
    }
    
    return true;
}

/*----------------------
     Server Service
----------------------*/

bool ServerService::start( ) {
    if ( !canStart( ) ) {
        return false;
    }

    auto service = std::static_pointer_cast<ServerService>( shared_from_this( ) );
	listener_->startAccept( service );

    return true;
}
