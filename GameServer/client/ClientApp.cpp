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
		onlineGame->setupStage();
		onlineGame->setTimer(pTimer);
	}
	else {
		std::cout << "Unknown game type. Type: " << static_cast<uint16>(type) << '\n';
		return;
	}

	SetWindowLongPtrA(ghWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(game_.get()));
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

std::unique_ptr<IGame> ClientApp::game_ = nullptr;
std::unique_ptr<ServerSession> ClientApp::serverSession_ = nullptr;

} // namespace INetwork