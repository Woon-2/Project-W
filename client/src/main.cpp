#include "game.hpp"

int APIENTRY WinMain(HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance,
    [[maybe_unused]] LPSTR lpCmdLine, [[maybe_unused]] int nCmdShow
) {
    gfx::d3d12engine::Core::setHInst(hInstance);
    gfx::DXInfoQueue::init();
    auto ret = Game().run();
    gfx::DXInfoQueue::cleanup();
    return ret;
}