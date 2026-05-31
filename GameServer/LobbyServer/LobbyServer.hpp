#ifndef lobby_server_hpp
#define lobby_server_hpp

#include "IocpReactor.hpp"
#include "Listener.hpp"

class LobbyServer {
public:
	LobbyServer() : reactor_(), listener_( reactor_.iocpHandle() ) {}

	void start();
	IocpReactor& reactor() { return reactor_; }

private:
	IocpReactor reactor_;
	Listener listener_;
};

#endif // lobby_server_hpp
