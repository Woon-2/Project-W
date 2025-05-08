#include "game.hpp"

#include "mouseWin32Adaptor.hpp"
#include "testInput.hpp"

#include <chrono>
#include <thread>

using namespace std::literals;

Game::Game()
    : session_(), core_(), mouse_(), keyboard_(), renderer_(core_), systems_(core_, &keyboard_, &mouse_),
    controllerAdapters_{ .renderModeController = RenderModeController(&renderer_) },
    timer_(), pStage_(), lockFPS_(defLockFPS) {
    setupWndMsgHandlers();
}

int Game::run() {
    try {

    initNetwork();

    pStage_ = std::make_unique<Stage>(core_, systems_, controllerAdapters_, renderer_, session_);

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

    net::relNet();

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
    pStage_->render();
}

void Game::initNetwork() {
    IDPool::initList();
    net::initNet();

    auto sock = net::TcpSocket( );
	if ( sock.nativeHandle( ) == INVALID_SOCKET ) {
		std::cerr << "socket failed\n";
		return;
	}

    sock.connect( net::SockAddr( net::Ipv4Addr( ), net::Port( PORT ) ) );

    session_ = Session(std::move(sock), -1);

    systems_.netSystem = CNetExSystem(&session_);

    session_.enqueuePacket(
        Packet{
            .size = calcPacketSize<CSHello>(),
            .type = PacketType::CSHello
        }
    );

    // non-blocking socket
    u_long mode = 1;
    if ( ioctlsocket(session_.sock().nativeHandle(), FIONBIO, &mode) == SOCKET_ERROR ) {
        std::cerr << "ioctlsocket failed\n";
        throw;
    }

    // disable nagle algorithm
    int flag = 1;
    setsockopt(session_.sock().nativeHandle(), IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag));

    session_.flushPackets();
}

void Game::setupWndMsgHandlers() {
    using MyWindow = gfx::d3d12engine::Core::MyWindow;
    core_.window().addMsgHandler(0, std::make_unique<TestInputHandler<MyWindow>>(core_.window(), mouse_, core_));
    core_.window().addMsgHandler(1, std::make_unique<ic::Win32::MouseMsgHandler<MyWindow>>(core_.window(), mouse_));
    core_.window().addMsgHandler( 10000, std::make_unique<
        Win32::BasicMsgHandler<gfx::d3d12engine::Core::MyWindow>
    >(core_.window()) );
}