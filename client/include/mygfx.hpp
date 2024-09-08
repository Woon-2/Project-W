#ifndef __MYGFX_HPP
#define __MYGFX_HPP

#include "d3d12core.hpp"

class MyGfx : public gfx::d3d12::Core {
public:
    enum class Renderer {
        PhongNT
    };

    void init() override;

    gfx::IRenderer& renderer(Renderer) const NOEXCEPT {
        return *pRenderer_;
    }

private:
    std::unique_ptr<gfx::IRenderer> pRenderer_;
};

#endif // __MYGFX_HPP