#ifndef __MYGFX_HPP
#define __MYGFX_HPP

#include "d3d12core.hpp"
#include "phongShader.hpp"

class MyGfx : public gfx::d3d12::Core {
public:
    enum class Renderer {
        Phong
    };

    void init() override;
    void setFrame(std::size_t frameIdx) {
        static_cast<gfx::d3d12::PhongShader&>(
            shader(gfx::d3d12::PhongShader::shaderName())
        ).setFrame(frameIdx);
    }

    gfx::IRenderer& renderer(Renderer) const NOEXCEPT {
        return *pRenderer_;
    }

private:
    std::unique_ptr<gfx::IRenderer> pRenderer_;
};

#endif // __MYGFX_HPP