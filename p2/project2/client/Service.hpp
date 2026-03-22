#ifndef SERVICE_HPP
#define SERVICE_HPP

#include "Listener.hpp"

/*---------------
     Service
---------------*/

enum class ServiceType : uint8 {
    Client,
    Server
};

using SessionFactory = std::function<SPSession( )>;

class Service : public std::enable_shared_from_this<Service> {
public:
    Service( ServiceType type, const NetAddress& netAddr,
        const SPIocpCore& iocpCore, const SessionFactory& factory, int32 maxSessionCount )
        : mtx_( ), type_( type ), netAddr_( netAddr ), iocpCore_( iocpCore ),
        factory_( factory ), sessions_( ), sessionCount_( 0 ), maxSessionCount_( maxSessionCount ) {}

    virtual ~Service( ) {}

    virtual bool start( ) = 0;
    bool canStart( ) {
        return factory_ != nullptr;
    }

    SPSession createSession( );
	void addSession( const SPSession& session );
	void releaseSession( const SPSession& session );

    void setSessionFactory( const SessionFactory& factory ) {
        factory_ = factory;
    }

    ServiceType getType( ) const {
        return type_;
	}

    const NetAddress& getNetAddress( ) const {
        return netAddr_;
    }

    SPIocpCore& getIocpCore( ) {
        return iocpCore_;
	}

    int32 getSessionCount( ) const {
        return sessionCount_;
    }

    int32 getMaxSessionCount( ) const {
        return maxSessionCount_;
	}

protected:
    std::mutex mtx_;    // temporary
    ServiceType type_;
    NetAddress netAddr_;
	SPIocpCore iocpCore_;

	SessionFactory factory_;
    std::set<SPSession> sessions_;
    int32 sessionCount_;
	int32 maxSessionCount_;
};

/*----------------------
     Client Service
----------------------*/

class ClientService : public Service {
public:
    ClientService( const NetAddress& targetNetAddr, const SPIocpCore& iocpCore,
        const SessionFactory& factory, int32 maxSessionCount )
        : Service( ServiceType::Client, targetNetAddr, iocpCore, factory, maxSessionCount ) {}

    virtual ~ClientService( ) {}

	virtual bool start( ) override;
};

/*----------------------
     Server Service
----------------------*/

class ServerService : public Service {
public:
    ServerService( const NetAddress& netAddr, const SPIocpCore& iocpCore,
        const SessionFactory& factory, int32 maxSessionCount )
        : Service( ServiceType::Server, netAddr, iocpCore, factory, maxSessionCount ),
		listener_( std::make_shared<Listener>( ) ) 
    {
		ASSERT_CRASH( listener_ != nullptr );
    }

    virtual ~ServerService( ) {}

    virtual bool start( ) override;

private:
    SPListener listener_;
};

#endif // SERVICE_HPP