#ifndef room_server_hpp
#define room_server_hpp

#include "IocpReactor.hpp"
#include "Listener.hpp"

class RoomServer {
public:
	explicit RoomServer(uint16 listenPort)
		: reactor_(), listener_(std::make_shared<Listener>(reactor_.iocpHandle(), listenPort)),
		iocpThreads_(), jobTimerThread_(), jobThreads_()
	{}

	void start();
	//void stop();

	IocpReactor& iocpReactor() { return reactor_; }

private:
	IocpReactor reactor_;
	std::shared_ptr<Listener> listener_;
	std::vector<std::thread> iocpThreads_;
	std::thread jobTimerThread_;
	std::vector<std::thread> jobThreads_;
};

#endif // room_server_hpp
