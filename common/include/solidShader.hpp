#ifndef __SOLIDSHADER_HPP
#define __SOLIDSHADER_HPP

#include "d3d12shader.hpp"

#include "shaderPath.hpp"
#include "rootPresets.hpp"
#include "d3d12InputLayoutPresets.hpp"

namespace gfx {

namespace d3d12 {

class SolidShader : public Shader {
public:
    SolidShader(Core& core);

    static constexpr std::string shaderName() {
        return "solidShader";
    }

    static Core::RootIdx rootName() {
        return d3d12::rootName(RootPreset::Solid);
    }

    static Core::InputLayoutIdx inputLayoutName() {
        return d3d12::inputLayoutName(InputLayoutPreset::Pos3);
    }
};

}   // namespace d3d12

}   // namespace gfx

#endif // __SOLIDSHADER_HPP