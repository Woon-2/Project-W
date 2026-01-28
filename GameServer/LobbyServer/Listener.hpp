#ifndef listener_hpp
#define listener_hpp

#include "IocpObject.hpp"
#include "Memory.hpp"

class IoEvent;
class AcceptEvent;
class Listener : public IocpObject {
public:
	Listener() : listenSock_(SocketUtils::createSocket()), acceptEvs_() {}

	virtual ~Listener() {
		SocketUtils::closeSocket(listenSock_);

		for(auto ev : acceptEvs_) {
			xdelete(ev);
		}
	}

	void startAccept();

	virtual void dispatch(IoEvent* event, int32 numBytes) override;
	virtual HANDLE getHandle() const { return reinterpret_cast<HANDLE>(listenSock_); }
	virtual SOCKET getSocket() const { return listenSock_; }

private:
	void registerAccept(AcceptEvent* acceptEv);
	void processAccept(AcceptEvent* acceptEv);

private:
	SOCKET listenSock_;
	std::vector<AcceptEvent*> acceptEvs_;
};

#endif	// listener_hpp