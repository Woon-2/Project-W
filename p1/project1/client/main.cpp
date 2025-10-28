#include "pch.hpp"
#include "errorHandling.hpp"
#include "gfx.hpp"
#include "object.hpp"
#include "timer.hpp"

inline constexpr const char* wndClsName = "wndCls";
inline constexpr const char* wndName = "Project1";

HWND ghWnd = nullptr;
RECT gWndRect{ 0, 0, 1024, 768 };
RECT gClientRect{ 0, 0, 1024, 768 };

LRESULT wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) 
{
	std::locale::global(std::locale("ko-KR"));

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

	auto threadPool = ThreadPool();
	threadPool.run(numberOfPhysicalCores() - 1u);
	std::cout << "ThreadPool runs with " << numberOfPhysicalCores() << " threads (the physical core count - 1)\n";

	// 그래픽스 초기화 - DXGI, D3D12
	GFX gfx{};
	gfx.setupDXGI(D3D_FEATURE_LEVEL_12_1);
	gfx.init();
	gfx.createSwapChain();

	gfx.loadMeshes();
	gfx.setThreadPool(&threadPool);

	auto cubes = std::vector<std::vector<std::vector<Object>>>(10u);
	for (auto& plane : cubes) {
		plane.resize(10u);
		for (auto& row : plane) {
			row.resize(10u);
		}
	}

	for (std::size_t i = 0u; i < cubes.size(); ++i) {
		for (std::size_t j = 0u; j < cubes[i].size(); ++j) {
			for (std::size_t k = 0u; k < cubes[i][j].size(); ++k) {
				cubes[i][j][k].setMesh(gfx.cubeMesh());
				cubes[i][j][k].setPos( mu::Vec3(
					(static_cast<int>(k) - static_cast<int>(cubes.size() / 2)) * 0.25f,
					(static_cast<int>(j) - static_cast<int>(cubes.size() / 2)) * 0.25f,
					(static_cast<int>(i) - static_cast<int>(cubes.size() / 2)) * 0.25f
				) );
				cubes[i][j][k].setOmega( mu::Vec3(rand(-1.f, 1.f), rand(-1.f, 1.f), rand(-1.f, 1.f)) );
				cubes[i][j][k].setScale(0.1f);
			}
			
		}
	}

	Timer timer{};

	// 윈도우 메시지 루프
	MSG msg;
	while (true) {
		while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				return static_cast<int>(msg.wParam);
			}

			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}

		timer.tick();

		for (auto& plane : cubes) {
			for (auto& row : plane) {
				for (auto& cube : row) {
					cube.update(timer.deltaTime<Milliseconds>());
				}
			}
		}

		for (auto& plane : cubes) {
			for (auto& row : plane) {
				for (auto& cube : row) {
					cube.render(gfx);
				}
			}
		}

		auto title = wndName + "(FPS: "s + std::to_string(timer.fps()) + ")"s;
		SetWindowTextA(ghWnd, title.c_str());

		gfx.render();
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