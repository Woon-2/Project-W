#ifndef __GAME_HPP
#define __GAME_HPP

#include "d3d12core.hpp"
#include "Timer.hpp"
#include "camera.hpp"
#include "coord.hpp"

#include "d3d12model.hpp"

#include <memory>

class Game {
public:
    static constexpr auto defLockFPS = 144.;
    using MyWindow = gfx::d3d12::Window<gfx::d3d12::BasicD3D12WTraits<char>>;

    Game()
        : timer_(), baseCoordSys_(), camera_(baseCoordSys_),
        pGfx_(), pWnd_(), pDrawable_(), lockFPS_(defLockFPS) {}
    
    Game(gfx::ICore& gfx, MyWindow& wnd);

    void update();
    void render();
    double fpsLock() const { return lockFPS_; }
    double setFPSLock(double fps) { return lockFPS_ = fps; }

private:
    void processInput();

    Timer timer_;

    gfx::coord::System baseCoordSys_;
    gfx::Camera camera_;
    std::unique_ptr<gfx::d3d12::Model> pDrawable_;
    gfx::ICore* pGfx_;
    MyWindow* pWnd_;
    double lockFPS_;
};

#endif // __GAME_HPP