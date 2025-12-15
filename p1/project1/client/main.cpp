#include "pch.hpp"
#include "errorHandling.hpp"
#include "global.hpp"
#include "online/onlineGame.hpp"
#include "standalone/game.hpp"
#include "timer.hpp"
#include "IocpCore.hpp"
#include "Service.hpp"
#include "ServerSession.hpp"

inline constexpr const char* wndClsName = "wndCls";
inline constexpr const char* wndName = "Project1";

HWND ghWnd = nullptr;
RECT gWndRect{ 0, 0, 1024, 768 };
RECT gClientRect{ 0, 0, 1024, 768 };

LRESULT wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
SPClientService tryConnectToServer();
std::thread makeIOCPLoopThread(SPClientService& clientService);

int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
	SocketUtils::init( );
	std::locale::global( std::locale( "ko-KR" ) );

	pushLoggerA("standard", &std::cout);
	pushLoggerW("standard", &std::wcout);

	// 윈도우 클래스 설정 및 윈도우 생성
	auto cls = WNDCLASSEXA{
		.cbSize = sizeof( WNDCLASSEXA ),
		.style = CS_OWNDC,
		.lpfnWndProc = wndProc,
		.cbClsExtra = 0,
		.cbWndExtra = 0,
		.hInstance = hInstance,
		.hIcon = nullptr,
		.hCursor = nullptr,
		.hbrBackground = nullptr,
		.lpszMenuName = nullptr,
		.lpszClassName = wndClsName,
		.hIconSm = nullptr
	};

	DISPLAY_ERROR_GLE( RegisterClassExA( &cls ), true );

	AdjustWindowRect( &gWndRect, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, false );

	ghWnd = CreateWindowExA( 0, wndClsName, wndName, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		0, 0, gWndRect.right - gWndRect.left, gWndRect.bottom - gWndRect.top,	// 윈도우의 시작 위치, 크기 설정
		nullptr, nullptr, hInstance, nullptr
	);

	DISPLAY_ERROR_GLE( ghWnd, true );
	DISPLAY_ERROR_GLE( !ShowWindow( ghWnd, SW_SHOW ), true );


	// 윈도우 프로시저에서 WM_INPUT 메시지 수신을 위한 Raw Input Device 등록(마우스)
	auto rid = RAWINPUTDEVICE{
        .usUsagePage = 0x01,    // Generic Desktop Controls
        .usUsage = 0x02,    // Mouse
        .dwFlags = 0,
        .hwndTarget = nullptr   // NULL for the whole system
    };

	DISPLAY_ERROR_GLE(RegisterRawInputDevices(&rid, 1, sizeof(rid)), true);


	// 게임 초기화
	Timer timer{};

	auto clientService = tryConnectToServer();
	ASSERT_CRASH( clientService != nullptr );
	std::thread iocpLoopThread{ makeIOCPLoopThread( clientService ) };
	
	while ( !gReady );

	switch ( pGame->type() ) {
	case GameType::StandAlone:
		static_cast<StandAlone::Game*>(pGame.get())->setTimer(&timer);
		break;

	case GameType::Online:
		break;

	default:
		DISPLAY_ERROR_STR(false, "[Initialization Error] 게임 타입은 StandAlone, Online 둘 중 하나여야 합니다.", true);
		break;
	}

	// 윈도우 메시지 루프
	MSG msg;
	while ( true ) {
		while ( PeekMessageA( &msg, nullptr, 0, 0, PM_REMOVE ) ) {
			if ( msg.message == WM_QUIT ) {
				if (clientService) {
					iocpLoopThread.join();
					SocketUtils::rel( );
				}
				return static_cast<int>( msg.wParam );
			}

			TranslateMessage( &msg );
			DispatchMessageA( &msg );
		}

		timer.tick( );
		auto title = wndName + "(FPS: "s + std::to_string( timer.fps( ) ) + ")"s;
		SetWindowTextA( ghWnd, title.c_str( ) );

		pGame->update(timer.deltaTime<Milliseconds>());
		pGame->render();
	}
}

LRESULT wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CLOSE:
		PostMessageA(ghWnd, WM_DESTROY, 0, 0);
		DISPLAY_ERROR_GLE(DestroyWindow(hWnd), true);
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	default:
		if (pGame) {
			return pGame->receiveWndMsg(hWnd, msg, wParam, lParam);
		}
		else {
			return DefWindowProcA(hWnd, msg, wParam, lParam);
		}
	}
}

SPClientService tryConnectToServer() {
	auto clientService = std::make_shared<ClientService>(
		NetAddress( serverIp, serverPort ), std::make_shared<IocpCore>( ),
		nullptr, 1 );

	clientService->setSessionFactory( []( ) {
		return std::make_shared<ServerSession>( );
	} );

	if ( clientService->start( ) ) {
		return clientService;
	}
	return nullptr;
}

std::thread makeIOCPLoopThread(SPClientService& clientService) {
	return std::thread( [&clientService]( ) {
		while ( true ) {
			if ( !clientService->getIocpCore( )->dispatch( ) ) {
				break;
			}
		}
	} );
}