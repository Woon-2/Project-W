#ifndef __GAME_HPP
#define __GAME_HPP

#include "stage.hpp"

#include "mouseWin32Adaptor.hpp"
#include "keyboardWin32Adaptor.hpp"
#include "d3d12engine/d3d12Engine.hpp"
#include "Timer.hpp"
#include "systems.hpp"

#include <vector>
#include <memory>

class Game {
public:
    static constexpr auto defLockFPS = 600.;
    using Mouse = ic::Win32::Mouse;
    using Keyboard = ic::Win32::Keyboard;
    
    Game();

    int run();

    double fpsLock() const { return lockFPS_; }
    double setFPSLock(double fps) { return lockFPS_ = fps; }

private:
    void update();
    void render();

    void setupWndMsgHandlers();
    void initialRender();
    void regularRender();

    gfx::d3d12engine::Core core_;

    Mouse mouse_;
    Keyboard keyboard_;

    Systems systems_;

    Timer timer_;

    std::unique_ptr<Stage> pStage_;

    double lockFPS_;
};

#endif // __GAME_HPP