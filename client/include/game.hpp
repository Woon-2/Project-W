#ifndef __GAME_HPP
#define __GAME_HPP

#include "d3d12core.hpp"
#include "Timer.h"

#include "sampleScene.hpp"

#include <memory>

class Game {
public:
    using MyWindow = gfx::d3d12::Window<gfx::d3d12::BasicD3D12WTraits<char>>;

    Game()
        : pGfx_(), pWnd_(), pDrawable_() {}
    
    Game(gfx::ICore& gfx, MyWindow& wnd);

    void update();
    void render();

private:
    void processInput();

    Timer timer_;

    gfx::ICore* pGfx_;
    MyWindow* pWnd_;

    std::unique_ptr<gfx::SampleDrawable> pDrawable_;
};

#endif // __GAME_HPP