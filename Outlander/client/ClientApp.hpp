#ifndef client_app_hpp
#define client_app_hpp

#include "ServerSession.hpp"
#include "networkConfig.hpp"

class IGame;
class SendBuffer;
class Timer;
class SoundManager;

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
	static void init(const NetworkEndpoint& lobbyEndpoint);
	static bool connectToServer() { return serverSession_->connect(); }
	// S_GameStart 핸드오프: 인증된 로비 세션은 유지하고 RoomServer를 활성 세션으로 전환한다.
	static void reconnectToRoomServer(const std::string& ip, uint16 port);
	// 게임 종료 후 유지 중인 인증 로비 세션을 다시 활성화한다.
	// 복귀할 로비 연결이 남아 있지 않으면 false를 반환한다.
	static bool returnToLobbyServer();
	static void release();
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

	// 프로세스 전역 오디오 엔진. init() 이후 항상 유효하다(디바이스 실패 시에도 객체는 존재하며 no-op).
	static SoundManager& sound();

	static const std::string& serverIp() { return serverSession_->ip(); }
	static uint16 serverPort() { return serverSession_->port(); }

private:
	static std::unique_ptr<IGame> game_;
	static std::unique_ptr<SoundManager> sound_;
	static std::unique_ptr<ServerSession> serverSession_;
	// 인게임 동안에도 인증/방 소속을 유지하는 LobbyServer 세션.
	static std::unique_ptr<ServerSession> lobbySession_;
	// 닫힌 이전 활성 세션. 잔여 완료 APC(완료 콜백의 owner 포인터)가 안전히 드레인되도록
	// 객체를 살려둔다(release()까지 보관, 1개라 무시 가능).
	// static 소멸에 맡기면 WSACleanup 이후 closesocket·정적 풀 소멸 순서 경합이 생기므로
	// 반드시 release()에서 함께 정리한다.
	static std::unique_ptr<ServerSession> retiredSession_;
};

} // namespace INetwork

#endif // client_app_hpp
