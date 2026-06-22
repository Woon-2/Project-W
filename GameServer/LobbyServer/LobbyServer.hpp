#ifndef lobby_server_hpp
#define lobby_server_hpp

#include "IocpReactor.hpp"
#include "Listener.hpp"

class LobbyServer {
public:
	explicit LobbyServer(uint16 listenPort)
		: reactor_(), listener_( std::make_shared<Listener>( reactor_.iocpHandle(), listenPort ) ) {}

	void start();
	IocpReactor& reactor() { return reactor_; }

private:
	IocpReactor reactor_;
	std::shared_ptr<Listener> listener_;
};

#endif // lobby_server_hpp
