#ifndef room_server_hpp
#define room_server_hpp

#include "IocpReactor.hpp"
#include "Listener.hpp"

class RoomServer {
public:
	RoomServer() : reactor_(), listener_(reactor_.iocpHandle()),
		iocpThreads_(), jobTimerThread_(), jobThreads_()
	{}

	void start();
	//void stop();

	IocpReactor& iocpReactor() { return reactor_; }

private:
	IocpReactor reactor_;
	Listener listener_;
	std::vector<std::thread> iocpThreads_;
	std::thread jobTimerThread_;
	std::vector<std::thread> jobThreads_;
};

#endif // room_server_hpp