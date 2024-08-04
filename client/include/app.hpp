#ifndef __APP_HPP
#define __APP_HPP

#include "game.hpp"

#include <memory>

class App {
public:
    using MyWindow = gfx::d3d12::Window<gfx::d3d12::BasicD3D12WTraits<char>>;

    App(HINSTANCE hInstance);
    ~App();

    App(const App&) = delete;
    App(App&&) noexcept = default;
    App& operator=(const App&) = delete;
    App& operator=(App&&) noexcept = default;

    int run();

private:
    MyWindow window_;
    std::unique_ptr<gfx::ICore> pGfx_;
    std::unique_ptr<Game> pGame_;
};

#endif // __APP_HPP