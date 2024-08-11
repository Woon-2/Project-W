#ifndef __GAME_HPP
#define __GAME_HPP

#include "d3d12core.hpp"

#include "sampleScene.hpp"

#include "player.hpp"

#include "keyboard.hpp"

#include "inputSystem.hpp"

#include <memory>

class Game {
public:
    using MyWindow = gfx::d3d12::Window<gfx::d3d12::BasicD3D12WTraits<char>>;

    Game()
        : pGfx_(), pWnd_(), pDrawable_(), player_(), keyManager_(5) {}
    
    Game(gfx::ICore& gfx, MyWindow& wnd);

    void update();
    void render();

private:
    Player player_;
    KeyBoard keyboard_;
    KeyManager keyManager_;

    InputSystem inputSystem_;

private:
    void processInput();

    gfx::ICore* pGfx_;
    MyWindow* pWnd_;

    std::unique_ptr<gfx::SampleDrawable> pDrawable_;
};

#endif // __GAME_HPP