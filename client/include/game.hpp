#ifndef __GAME_HPP
#define __GAME_HPP

#include "mouseWin32Adaptor.hpp"
#include "keyboardWin32Adaptor.hpp"

#include "d3d12core.hpp"
#include "Timer.hpp"
#include "camera.hpp"
#include "coord.hpp"

#include "d3d12model.hpp"

#include "player.hpp"

#include "inputSystem.hpp"
#include "physicsSystem.hpp"

#include <vector>
#include <memory>

#include "gun.hpp"
#include "shaderRes.hpp"

class Game {
public:
    static constexpr auto defLockFPS = 600.;
    using MyWindow = gfx::d3d12::Window<gfx::d3d12::BasicD3D12WTraits<char>>;
    using RenderFunc = void (Game::*)();

    Game()
        : timer_(), baseCoordSys_(), camera_(baseCoordSys_),
        /* inputSystem_() ,*/ physicsSystem_(), pGfx_(), pWnd_(), pMouse_(),
        renderFunc_(&Game::initialRender), lockFPS_(defLockFPS), player_(),
        curFenceIdx_(0), prevFenceIdx_(1u) {}
    
    Game(gfx::ICore& gfx, MyWindow& wnd, ic::Mouse& mouse, ic::Keyboard& keyboard);

    void update();
    void render();
    double fpsLock() const { return lockFPS_; }
    double setFPSLock(double fps) { return lockFPS_ = fps; }

private:
    void setupWndMsgHandlers();
    void processInput();
    void loadAssets();
    void setupCamera();
    void initECS();
    void initialRender();
    void regularRender();
    void processNetwork();
    void initLights();

    Timer timer_;
    gfx::coord::System baseCoordSys_;
    gfx::Camera camera_;
    std::vector<gfx::d3d12::sr::PhongLight> lights_;
    PhysicsSystem physicsSystem_;
    gfx::ICore* pGfx_;
    MyWindow* pWnd_;
    ic::Mouse* pMouse_;
    RenderFunc renderFunc_;
    double lockFPS_;
    Player player_;
    std::size_t curFenceIdx_;
    std::size_t prevFenceIdx_;
};

#endif // __GAME_HPP