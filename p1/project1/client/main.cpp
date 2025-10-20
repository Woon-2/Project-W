#include "pch.hpp"
#include "errorHandling.hpp"
#include "gfx.hpp"

inline constexpr const char* wndClsName = "wndCls";
inline constexpr const char* wndName = "Project1";

HWND ghWnd = nullptr;
RECT gWndRect{ 0, 0, 1024, 768 };
RECT gClientRect{ 0, 0, 1024, 768 };

LRESULT wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) 
{
	// 윈도우 클래스 설정 및 윈도우 생성
	auto cls = WNDCLASSEXA{
		.cbSize = sizeof(WNDCLASSEXA),
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

	DISPLAY_ERROR_GLE(RegisterClassExA(&cls), true);

	AdjustWindowRect(&gWndRect, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, false);

	ghWnd = CreateWindowExA(0, wndClsName, wndName, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		0, 0, gWndRect.right - gWndRect.left, gWndRect.bottom - gWndRect.top,	// 윈도우의 시작 위치, 크기 설정
		nullptr, nullptr, hInstance, nullptr
	);

	DISPLAY_ERROR_GLE(ghWnd, true);
	DISPLAY_ERROR_GLE(!ShowWindow(ghWnd, SW_SHOW), true);

	// 그래픽스 초기화 - DXGI, D3D12
	GFX gfx{};
	gfx.setupDXGI(D3D_FEATURE_LEVEL_12_1);
	gfx.init(1);
	gfx.createSwapChain(ghWnd);

	// 윈도우 메시지 루프
	MSG msg;
	while (true) {
		while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				return static_cast<int>(msg.wParam);
			}

			gfx.render();

			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
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
		return DefWindowProcA(hWnd, msg, wParam, lParam);
	}
}