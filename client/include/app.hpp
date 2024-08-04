#include "d3d12core.hpp"

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
    std::unique_ptr<gfx::ICore> gfx_;
    MyWindow window_;
};