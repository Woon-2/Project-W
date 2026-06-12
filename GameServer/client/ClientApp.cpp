#include "pch.hpp"
#include "ClientApp.hpp"
#include "standalone/game.hpp"
#include "online/onlineGame.hpp"

namespace INet {

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
	if (serverSession_) {
		serverSession_->close();
	}
	if (retiredSession_) {
		retiredSession_->close();
	}

	SetWindowLongPtrA(ghWnd, GWLP_USERDATA, 0);
	game_.reset();
	serverSession_.reset();
	retiredSession_.reset();
}

void ClientApp::update(Milliseconds deltaTime) {
	game_->update(deltaTime);
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
std::unique_ptr<ServerSession> ClientApp::serverSession_ = nullptr;
std::unique_ptr<ServerSession> ClientApp::retiredSession_ = nullptr;

} // namespace INetwork
