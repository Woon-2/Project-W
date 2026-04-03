#include "pch.hpp"
#include "errorHandling.hpp"
#include "online/onlineGame.hpp"
#include "standalone/game.hpp"
#include "timer.hpp"
#include "SendBuffer.hpp"
#include "ClientApp.hpp"

inline constexpr const char* wndClsName = "wndCls";
inline constexpr const char* wndName = "Project1";

HWND ghWnd = nullptr;
RECT gWndRect{ 0, 0, 1024, 768 };
RECT gClientRect{ 0, 0, 1024, 768 };
bool gClose = false;

LRESULT wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
	SocketUtils::init( );
	MemoryManager::init();
	INet::ClientApp::init();

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

	std::cout << "[Main] 서버 연결 중...\n";
	if (!INet::ClientApp::connectToServer()) {
		std::cout << "[Main] 서버 연결 실패\n" << "StandAlone 모드로 실행합니다.\n";
		INet::ClientApp::setup(INet::GameType::StandAlone, &timer);
	}
	else {
		std::cout << "[Main] 서버 연결 성공\n" << "서버 IP: " << INet::ClientApp::serverIp() << ", 서버 Port: " << INet::ClientApp::serverPort() << '\n';
		INet::ClientApp::setup(INet::GameType::Online, &timer);
	}
	
	// 윈도우 메시지 루프
	MSG msg;
	while ( true ) {
		while ( PeekMessageA( &msg, nullptr, 0, 0, PM_REMOVE ) ) {
			if ( msg.message == WM_QUIT ) {
				gClose = true;
				SendBufferManager::release();
				MemoryManager::release();
				SocketUtils::release();
				return static_cast<int>( msg.wParam );
			}

			TranslateMessage( &msg );
			DispatchMessageA( &msg );
		}

		timer.tick( );
		auto title = wndName + "(FPS: "s + std::to_string( timer.fps( ) ) + ")"s;
		SetWindowTextA( ghWnd, title.c_str( ) );

		INet::ClientApp::update(timer.deltaTime<Milliseconds>());
		INet::ClientApp::render();
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
