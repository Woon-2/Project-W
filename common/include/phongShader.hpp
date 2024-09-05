#ifndef __PHONGSHADER_HPP
#define __PHONGSHADER_HPP

#include "d3d12shader.hpp"
#include "d3d12res.hpp"

#include "shaderPath.hpp"
#include "rootPresets.hpp"
#include "d3d12InputLayoutPresets.hpp"

#include <vector>

namespace gfx {

namespace d3d12 {

	class PhongShaderNT : public Shader {
    public:
        static constexpr size_t defMaxInstances = 1000u;
        static constexpr size_t defMaxMaterials = 24u;
        static constexpr size_t defMaxLights = 12u;

        PhongShaderNT(Core& core, std::size_t duplicationCnt = 1u);

        static constexpr std::string shaderName() NOEXCEPT {
            return "PhongShaderNT";
        }

        static Core::RootIdx rootName() NOEXCEPT {
            return d3d12::rootName(RootPreset::Unified1);
        }

        static Core::InputLayoutIdx inputLayoutName() NOEXCEPT {
            return d3d12::inputLayoutName(InputLayoutPreset::Pos3Norm3Tex2);
        }

        GpuMappedRes& pfd() NOEXCEPT { return resPerFrameData_; }
        GpuMappedRes& pdd() NOEXCEPT { return resPerDrawcallData_; }
        GpuMappedRes& pid() NOEXCEPT { return resPerInstanceData_; }
        GpuMappedRes& materials() NOEXCEPT { return resMaterials_; }
        GpuMappedRes& lights() NOEXCEPT { return resLights_; }

    private:
        std::vector< std::vector< wrl::ComPtr<ID3D12Resource> > > internalResArr_;

        GpuMappedRes resPerFrameData_;
        GpuMappedRes resPerDrawcallData_;
        GpuMappedRes resPerInstanceData_;
        GpuMappedRes resMaterials_;
        GpuMappedRes resLights_;

        size_t maxInstances_;
        size_t maxMaterials_;
        size_t maxLights_;
	};


}	// namespace gfx::d3d12

}	// namespace gfx

#endif // !__PHONGSHADER_HPP
