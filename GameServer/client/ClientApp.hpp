#ifndef client_app_hpp
#define client_app_hpp

#include "ServerSession.hpp"

class IGame;
class SendBuffer;
class Timer;

namespace Online { class Game; }

namespace INet {

enum class GameType {
	StandAlone,
	Online
};

/***
* @brief SingletonBase
*/
class ClientApp {
public:
	static void init(){ serverSession_ = std::make_unique<ServerSession>();	}
	static bool connectToServer() { return serverSession_->connect(); }
	// Online 모드가 아닐 땐 사용하지 않도록 한다.
	static void addSendBuffer(const std::shared_ptr<SendBuffer>& sendBuffer) { serverSession_->addSendBuffer(sendBuffer); }
	// Online 모드가 아닐 땐 사용하지 않도록 한다.
	static void send() { serverSession_->send(); }

	static void setup(GameType type, Timer* pTimer);
	// setup이 호출되기 전까지는 호출하지 않도록 한다.
	static void update(Milliseconds deltaTime);
	// setup이 호출되기 전까지는 호출하지 않도록 한다.
	static void render();

	// setup이 호출되기 전까지는 호출하지 않도록 한다.
	// Online 모드가 아닐 땐 사용하지 않도록 한다.
	static Online::Game* onlineGame();

	static const std::string& serverIp() { return serverSession_->ip(); }
	static uint16 serverPort() { return serverSession_->port(); }

private:
	static std::unique_ptr<IGame> game_;
	static std::unique_ptr<ServerSession> serverSession_;
};

} // namespace INetwork

#endif // client_app_hpp