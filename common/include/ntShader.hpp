#ifndef __ntShader_HPP
#define __ntShader_HPP

#include "d3d12shader.hpp"

#include "shaderPath.hpp"
#include "rootPresets.hpp"
#include "d3d12InputLayoutPresets.hpp"

namespace gfx {

namespace d3d12 {

class NTShader : public Shader {
public:
    NTShader(Core& core);

    static constexpr std::string shaderName() {
        return "NTShader";
    }

    static Core::RootIdx rootName() {
        return d3d12::rootName(RootPreset::Unified);
    }

    static Core::InputLayoutIdx inputLayoutName() {
        return d3d12::inputLayoutName(InputLayoutPreset::Pos3Norm3);
    }
};

}   // namespace d3d12

}   // namespace gfx

#endif // __ntShader_HPP