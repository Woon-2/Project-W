#include "app.hpp"

int APIENTRY WinMain(HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance,
    [[maybe_unused]] LPSTR lpCmdLine, [[maybe_unused]] int nCmdShow
) {
    return App(hInstance).run();
}