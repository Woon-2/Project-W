#ifndef __GAME_HPP
#define __GAME_HPP

#include "mouseWin32Adaptor.hpp"
#include "keyboardWin32Adaptor.hpp"

#include "d3d12core.hpp"
#include "Timer.hpp"

#include "systems.hpp"
#include "stage.hpp"

#include <vector>
#include <memory>

class Game {
public:
    static constexpr auto defLockFPS = 600.;
    using MyWindow = gfx::d3d12::Window<gfx::d3d12::BasicD3D12WTraits<char>>;
    using RenderFunc = void (Game::*)();
    
    Game(gfx::ICore& gfx, MyWindow& wnd, ic::Mouse& mouse, ic::Keyboard& keyboard);

    void update();
    void render();
    double fpsLock() const { return lockFPS_; }
    double setFPSLock(double fps) { return lockFPS_ = fps; }

private:
    void setupWndMsgHandlers();
    void initialRender();
    void regularRender();

    Systems systems_;
    Timer timer_;
    std::unique_ptr<Stage> pStage_;
    gfx::ICore* pGfx_;
    MyWindow* pWnd_;
    ic::Mouse* pMouse_;
    RenderFunc renderFunc_;
    double lockFPS_;
    std::size_t curFenceIdx_;
    std::size_t prevFenceIdx_;
};

#endif // __GAME_HPP