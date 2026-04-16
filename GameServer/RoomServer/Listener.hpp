#ifndef listener_hpp
#define listener_hpp

#include "IocpDispatchable.hpp"

class AcceptEvent;

class Listener : public IocpDispatchable {
public:
	Listener(HANDLE iocpHandle) : listenSock_(SocketUtils::createSocket()), acceptEvs_(), iocpHandle_(iocpHandle) {}

	~Listener() {
		SocketUtils::closeSocket(listenSock_);
		for (auto ev : acceptEvs_) {
			ObjectPool<AcceptEvent>::push(ev);
		}
	}

	void startAccept();
	virtual void dispatch(IoEvent* event, int32 numBytes) override;

	virtual HANDLE getHandle() const override { return reinterpret_cast<HANDLE>(listenSock_); }
	virtual SOCKET getSocket() const override { return listenSock_; }

private:
	void registerAccept(AcceptEvent* acceptEv);
	void processAccept(AcceptEvent* acceptEv);

private:
	SOCKET listenSock_;
	std::vector<AcceptEvent*> acceptEvs_;
	HANDLE iocpHandle_;
};

#endif // listener_hpp