#ifndef __GAME_HPP
#define __GAME_HPP

#include "d3d12core.hpp"
#include "Timer.h"

class Game {
public:
    using MyWindow = gfx::d3d12::Window<gfx::d3d12::BasicD3D12WTraits<char>>;

    Game()
        : pGfx_(nullptr), pWnd_(nullptr) {}
    
    Game(gfx::ICore& gfx, MyWindow& wnd)
        : pGfx_(&gfx), pWnd_(&wnd) {}

    void update();
    void render();

private:
    void processInput();

    Timer timer_;

    gfx::ICore* pGfx_;
    MyWindow* pWnd_;
};

#endif // __GAME_HPP