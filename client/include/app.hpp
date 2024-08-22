#ifndef __APP_HPP
#define __APP_HPP

#include "game.hpp"

#include <memory>

class App {
public:
    using MyWindow = gfx::d3d12::Window<gfx::d3d12::BasicD3D12WTraits<char>>;
    using MyMouse = ic::Win32::Mouse;
    using MyKeyboard = ic::Win32::Keyboard;

    App(HINSTANCE hInstance);
    ~App();

    App(const App&) = delete;
    App(App&&) noexcept = delete;
    App& operator=(const App&) = delete;
    App& operator=(App&&) noexcept = delete;

    int run();

private:
    MyWindow window_;
    MyMouse mouse_;
    MyKeyboard keyboard_;
    std::unique_ptr<gfx::ICore> pGfx_;
    std::unique_ptr<Game> pGame_;
};

#endif // __APP_HPP