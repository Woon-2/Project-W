#ifndef listener_hpp
#define listener_hpp

#include "IocpDispatchable.hpp"
#include "MemoryManager.hpp"

class AcceptEvent;

class Listener : public IocpDispatchable {
public:
	Listener() : listenSock_(SocketUtils::createSocket()), acceptEvs_() {}

	~Listener() {
		SocketUtils::closeSocket(listenSock_);
		for (auto ev : acceptEvs_) {
			odelete(ev);
		}
	}

	void startAccept();
	virtual void dispatch(IoEvent* event, int32 numBytes) override;

	virtual HANDLE getHandle() const override { return reinterpret_cast<HANDLE>(listenSock_); }

private:
	void registerAccept(AcceptEvent* acceptEv);
	void processAccept(AcceptEvent* acceptEv);

private:
	SOCKET listenSock_;
	std::vector<AcceptEvent*> acceptEvs_;
};

#endif // listener_hpp