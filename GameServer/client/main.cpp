#include "pch.hpp"
#include "errorHandling.hpp"
#include "online/onlineGame.hpp"
#include "standalone/game.hpp"
#include "timer.hpp"
#include "ServerSession.hpp"

inline constexpr const char* wndClsName = "wndCls";
inline constexpr const char* wndName = "Project1";

HWND ghWnd = nullptr;
RECT gWndRect{ 0, 0, 1024, 768 };
RECT gClientRect{ 0, 0, 1024, 768 };

LRESULT wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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
		.cbWndExtra = sizeof( IGame* ),
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


	// 서버 연결 및 게임 초기화
	Timer timer{};

	std::unique_ptr<IGame> pGame = nullptr;

	ServerSession serverSession;

	std::cout << "[Main] 서버 연결 중...\n";
	if (!serverSession.connect()) {
		std::cout << "[Main] 서버 연결 실패\n" << "StandAlone 모드로 실행합니다.\n";
		pGame = std::make_unique<StandAlone::Game>();

		SetWindowLongPtrA(ghWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pGame.get()));

		auto standAloneGame = static_cast<StandAlone::Game*>(pGame.get());
		standAloneGame->setupStage();
		standAloneGame->setTimer(&timer);
	}
	else {
		std::cout << "[Main] 서버 연결 성공\n" << "서버 IP: " << serverSession.ip() << ", 서버 Port: " << serverSession.port() << '\n';
		//pGame = std::make_unique<Online::Game>(serverSocket);
		
		//auto onlineGame = static_cast<Online::Game*>(pGame.get());
		//onlineGame->setupStage();
		//onlineGame->setTimer(&timer);
		while (true);
	}
	
	// 윈도우 메시지 루프
	MSG msg;
	while ( true ) {
		while ( PeekMessageA( &msg, nullptr, 0, 0, PM_REMOVE ) ) {
			if ( msg.message == WM_QUIT ) {
				SocketUtils::release( );
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
	auto pGame = reinterpret_cast<IGame*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

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
