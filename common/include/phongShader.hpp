#ifndef __PHONGSHADER_HPP
#define __PHONGSHADER_HPP

#include "d3d12shader.hpp"

#include "shaderPath.hpp"
#include "rootPresets.hpp"
#include "d3d12InputLayoutPresets.hpp"

namespace gfx {

namespace d3d12 {

	class PhongShader : public Shader {
    public:
        PhongShader(Core& core);

        static constexpr std::string shaderName() {
            return "PhongShader";
        }

        static Core::RootIdx rootName() {
            return d3d12::rootName(RootPreset::Unified);
        }

        static Core::InputLayoutIdx inputLayoutName() {
            return d3d12::inputLayoutName(InputLayoutPreset::Pos3Norm3);
        }
	};


}	// namespace gfx::d3d12

}	// namespace gfx

#endif // !__PHONGSHADER_HPP
