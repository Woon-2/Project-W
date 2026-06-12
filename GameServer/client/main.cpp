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

// 윈도우 스타일(생성 시와 동일해야 AdjustWindowRect 결과가 맞다).
inline constexpr DWORD kWndStyle = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

// 런타임 디스플레이 모드 변경: 창모드/전체화면(borderless)을 전환하며 윈도우 스타일·크기와
// 전역 RECT(gWndRect/gClientRect)를 갱신한다. 결과 클라이언트 크기를 out*에 돌려준다.
// GFX::resize 호출 전에 불러야 한다(뷰포트/깊이버퍼가 gClientRect를 읽기 때문). 메인 스레드 전용.
//
// 전체화면은 exclusive(SetFullscreenState)가 아니라 borderless(WS_POPUP + 모니터 전체 덮기)다.
// flip-model 스왑체인과 잘 맞고 alt-tab/모드전환 문제가 없으며, GFX::resize를 그대로 재사용한다.
// 현재 윈도우가 걸쳐 있는 모니터의 전체 해상도를 돌려준다(해상도 목록 필터링용).
void getCurrentMonitorSize(int* outW, int* outH) {
	HMONITOR mon = MonitorFromWindow(ghWnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO mi{ sizeof(MONITORINFO) };
	GetMonitorInfo(mon, &mi);
	if (outW) *outW = mi.rcMonitor.right - mi.rcMonitor.left;
	if (outH) *outH = mi.rcMonitor.bottom - mi.rcMonitor.top;
}

void applyDisplayMode(bool fullscreen, int windowedW, int windowedH, int* outClientW, int* outClientH) {
	if (fullscreen) {
		// 현재 윈도우가 걸쳐 있는 모니터의 전체 영역을 덮는다.
		HMONITOR mon = MonitorFromWindow(ghWnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFO mi{ sizeof(MONITORINFO) };
		GetMonitorInfo(mon, &mi);
		const int w = mi.rcMonitor.right - mi.rcMonitor.left;
		const int h = mi.rcMonitor.bottom - mi.rcMonitor.top;

		SetWindowLongPtr(ghWnd, GWL_STYLE, static_cast<LONG_PTR>(WS_POPUP | WS_VISIBLE));
		SetWindowPos(ghWnd, HWND_TOP,
			mi.rcMonitor.left, mi.rcMonitor.top, w, h,
			SWP_FRAMECHANGED | SWP_NOACTIVATE
		);

		gClientRect = RECT{ 0, 0, w, h };
		gWndRect    = RECT{ 0, 0, w, h };
		if (outClientW) *outClientW = w;
		if (outClientH) *outClientH = h;
	} else {
		SetWindowLongPtr(ghWnd, GWL_STYLE, static_cast<LONG_PTR>(kWndStyle | WS_VISIBLE));

		gClientRect = RECT{ 0, 0, windowedW, windowedH };
		RECT wr{ 0, 0, windowedW, windowedH };
		AdjustWindowRect(&wr, kWndStyle, false);
		gWndRect = wr;

		SetWindowPos(ghWnd, nullptr, 0, 0,
			wr.right - wr.left, wr.bottom - wr.top,
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED
		);
		if (outClientW) *outClientW = windowedW;
		if (outClientH) *outClientH = windowedH;
	}
}

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
	
	timer.tick( );

	// 윈도우 메시지 루프
	MSG msg;
	while ( true ) {
		while ( PeekMessageA( &msg, nullptr, 0, 0, PM_REMOVE ) ) {
			if ( msg.message == WM_QUIT ) {
				gClose = true;
				INet::ClientApp::release();			// 게임·세션 파괴(워커 스레드 join 포함)
				SendBufferManager::release();		// TLS 청크 해제 + 정적 큐 drain (정적 소멸 UB 방지)
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
