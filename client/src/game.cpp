#include "game.hpp"

#include "player.hpp"

#include "mouseWin32Adaptor.hpp"
#include "testInput.hpp"

#include <chrono>
#include <thread>

using namespace std::literals;

Game::Game()
    : session_(), core_(), mouse_(), keyboard_(), renderer_(core_), systems_(keyboard_),
    timer_(), pStage_(), lockFPS_(defLockFPS) {
    setupWndMsgHandlers();
}

int Game::run() {
    try {

    initNetwork();

    pStage_ = std::make_unique<Stage>(core_, systems_, renderer_, session_);

    for(;;) {
        if (auto returnCode = core_.window().processMessages()) {
            return returnCode.value();
        }

        update();
        render();
    }

    } catch (const gfx::Exception& e) {
        MessageBoxA(nullptr, e.what(), "GFX Exception",
            MB_OK | MB_ICONEXCLAMATION);
    } catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Standard Exception",
            MB_OK | MB_ICONEXCLAMATION);
    } catch(...) {
        MessageBoxA(nullptr, "no details available",
            "Unknown Exception", MB_OK | MB_ICONEXCLAMATION);
    }

    return 0;
}

void Game::update() {
    timer_.update();

    const auto expectedFrameTime = 1. / lockFPS_;
    const auto restFrameTime = expectedFrameTime - timer_.GetDT();

    if (restFrameTime > 0.) {
        std::this_thread::sleep_for( std::chrono::duration<double>(restFrameTime) );
    }

    pStage_->update(timer_.GetDT());
}

void Game::render() {
    pStage_->render(core_);
}

void Game::initNetwork() {
    WSADATA wsaData;
    if (::WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return;
    }

    auto sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "socket failed\n";
        return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    ::inet_pton(AF_INET, SERVERIP, &serverAddr.sin_addr);
    serverAddr.sin_port = ::htons(PORT);

    if (::connect(sock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "connect failed\n";
        return;
    }

    session_ = Session(sock);

    session_.enqueuePacket(
        Packet{
            .type = PacketType::CSHello,
            .size = sizeof(CSHello)
        }
    );
}

void Game::setupWndMsgHandlers() {
    using MyWindow = gfx::d3d12engine::Core::MyWindow;
    core_.window().addMsgHandler(0, std::make_unique<TestInputHandler<MyWindow>>(core_.window(), mouse_, core_));
    core_.window().addMsgHandler(1, std::make_unique<ic::Win32::MouseMsgHandler<MyWindow>>(core_.window(), mouse_));
    core_.window().addMsgHandler( 10000, std::make_unique<
        Win32::BasicMsgHandler<gfx::d3d12engine::Core::MyWindow>
    >(core_.window()) );
}