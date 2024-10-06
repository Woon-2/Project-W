#ifndef __NULLSHADER_HPP
#define __NULLSHADER_HPP

#include "d3d12shader.hpp"

#include "shaderPath.hpp"
#include "rootPresets.hpp"
#include "d3d12InputLayoutPresets.hpp"

namespace gfx {

namespace d3d12 {

class NullShader : public Shader {
public:
    NullShader(Core& core);

    static constexpr std::string shaderName() {
        return "nullShader";
    }

    static Core::RootIdx rootName() {
        return d3d12::rootName(RootPreset::Null);
    }

    void draw( IRenderContext& ctx, const IScene& scene,
        IRenderTarget& target, rp::Protocol protocol
    ) override;
};

}   // namespace d3d12

}   // namespace gfx

#endif // __NULLSHADER_HPP