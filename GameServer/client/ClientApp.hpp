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
	static void init(){ serverSession_ = std::make_unique<ServerSession>(::serverIp, lobbyServerPort);	}
	static bool connectToServer() { return serverSession_->connect(); }
	// S_GameStart 핸드오프: 로비 세션을 은퇴시키고 RoomServer로 새 세션을 맺는다.
	static void reconnectToRoomServer(const std::string& ip, uint16 port);
	static void release() { game_.reset(); serverSession_.reset(); }
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
	// 은퇴한 로비 세션. 닫힌 소켓의 잔여 완료 APC(완료 콜백의 owner 포인터)가 안전히 드레인되도록
	// 객체를 살려둔다(프로세스 수명 동안 보관, 1개라 무시 가능).
	static std::unique_ptr<ServerSession> retiredSession_;
};

} // namespace INetwork

#endif // client_app_hpp