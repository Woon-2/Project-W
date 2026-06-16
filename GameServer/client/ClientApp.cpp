#include "pch.hpp"
#include "ClientApp.hpp"
#include "standalone/game.hpp"
#include "online/onlineGame.hpp"
#include "sound/soundManager.hpp"

extern bool gClose;

namespace INet {

void ClientApp::init() {
	// 프로세스 전역 오디오 엔진을 1회 연다. 실패해도 객체는 유지되어 모든 호출이 안전한 no-op이 된다.
	sound_ = std::make_unique<SoundManager>();
	sound_->init();

	serverSession_ = std::make_unique<ServerSession>(::serverIp, lobbyServerPort);

	// std::exit() 경로 보호. 에러 매크로 등이 프레임 도중 std::exit를 부르면 정적
	// 소멸자들이 임의 순서로 돌면서, ~ServerSession이 보유한 SendBuffer를 이미
	// 파괴된 SendBufferManager 풀(다른 TU의 정적 객체)에 반납하다 AV가 났다.
	// WinMain(런타임)에서 등록한 atexit 핸들러는 pre-main에 등록된 모든 정적
	// 소멸자보다 먼저 실행되므로, 풀이 살아있는 동안 세션을 정리할 수 있다.
	// 정상 종료 경로에서는 release()가 이미 호출된 뒤라 no-op이다(멱등).
	std::atexit([]() {
		gClose = true;
		release();
	});
}

void ClientApp::setup(GameType type, Timer* pTimer) {
	if (GameType::StandAlone == type) {
		game_ = std::make_unique<StandAlone::Game>();

		auto standAloneGame = static_cast<StandAlone::Game*>(game_.get());
		standAloneGame->setupStage();
		standAloneGame->setTimer(pTimer);
	}
	else if (GameType::Online == type) {
		game_ = std::make_unique<Online::Game>();

		auto onlineGame = static_cast<Online::Game*>(game_.get());
		onlineGame->setTimer(pTimer);
		// setupStage(인게임)는 로비에서 게임 시작 시 호출된다.
		// 실행 직후엔 최소 리소스로 로비 씬에 진입한다.
		onlineGame->enterLobby();
	}
	else {
		std::cout << "Unknown game type. Type: " << static_cast<uint16>(type) << '\n';
		return;
	}

	SetWindowLongPtrA(ghWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(game_.get()));
}

void ClientApp::release() {
	SetWindowLongPtrA(ghWnd, GWLP_USERDATA, 0);

	game_.reset();                                         // 게임/워커 먼저(추가 send 차단)
	if ( serverSession_ )  serverSession_->closeAndDrain();  // 소켓 닫고 잔여 완료 APC 드레인
	if ( retiredSession_ ) retiredSession_->closeAndDrain();
	serverSession_.reset();                                // 이제 overlapped 안전 파괴
	retiredSession_.reset();

	// 게임 파괴 이후(잔여 SFX 트리거가 더 없는 시점) 오디오 엔진을 정리한다.
	sound_.reset();
}

void ClientApp::update(Milliseconds deltaTime) {
	game_->update(deltaTime);
	if (sound_) sound_->update(Seconds(deltaTime).count());
}

SoundManager& ClientApp::sound() {
	return *sound_;
}

void ClientApp::render() {
	game_->render();
}

Online::Game* ClientApp::onlineGame() {
	return static_cast<Online::Game*>(game_.get());
}

void ClientApp::reconnectToRoomServer(const std::string& ip, uint16 port) {
	// 옛 로비 세션을 은퇴 보관(닫고 살려둠 → 잔여 APC가 유효한 owner로 드레인되게).
	if (serverSession_) {
		serverSession_->close();
		retiredSession_ = std::move(serverSession_);
	}

	serverSession_ = std::make_unique<ServerSession>(ip, port);
	serverSession_->setGame(onlineGame());
	if (!serverSession_->connect()) {
		std::cout << "[Handoff] RoomServer 연결 실패: " << ip << ":" << port << '\n';
	}
}

std::unique_ptr<IGame> ClientApp::game_ = nullptr;
std::unique_ptr<SoundManager> ClientApp::sound_ = nullptr;
std::unique_ptr<ServerSession> ClientApp::serverSession_ = nullptr;
std::unique_ptr<ServerSession> ClientApp::retiredSession_ = nullptr;

} // namespace INetwork
