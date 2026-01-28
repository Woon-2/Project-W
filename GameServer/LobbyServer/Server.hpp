#ifndef lobby_server_hpp
#define lobby_server_hpp

#include "IocpCore.hpp"
#include "Listener.hpp"

class Server {
public:
	static void start();
	static void stop();

	static IocpCore& iocpCore() { return iocpCore_; }

private:
	static IocpCore iocpCore_;
	static Listener* listener_;
};

#endif	// lobby_server_hpp