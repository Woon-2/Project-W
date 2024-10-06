#ifndef __MYGFX_HPP
#define __MYGFX_HPP

#include "d3d12core.hpp"
#include "phongShader.hpp"
#include "renderer.hpp"

class MyGfx : public gfx::d3d12::Core {
public:
    using MyRenderer  = gfx::Renderer<gfx::d3d12::Shader>;
    void init() override;
    void setFrame(std::size_t frameIdx);
    const MyRenderer& illuminanceRenderer() const NOEXCEPT { return illuminanceRenderer_; }

private:
    std::optional<gfx::d3d12::PhongShader> phongShader_;
    std::optional<gfx::d3d12::PhongShaderNT> phongShaderNT_;
    MyRenderer illuminanceRenderer_;
};

#endif // __MYGFX_HPP