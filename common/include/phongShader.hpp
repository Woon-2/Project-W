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
    static constexpr size_t defMaxLights = 12u;

    struct Config {
        size_t maxInstances = defMaxInstances;
        size_t maxLights = defMaxLights;
    };

    PhongShaderNT(Core& core, const Config& config = Config{}, std::size_t duplicationCnt = 1u);

    static constexpr std::string shaderName() NOEXCEPT {
        return "PhongShaderNT";
    }

    static Core::RootIdx rootName() NOEXCEPT {
        return d3d12::rootName(RootPreset::Unified);
    }

    static Core::InputLayoutIdx inputLayoutName() NOEXCEPT {
        return d3d12::inputLayoutName(InputLayoutPreset::Pos3Norm3);
    }

    // TODO: make interface standard or base class
    // If we gonna make interface standard, we need to add requires expression on d3d12 drawers.
    void setRootParams(ID3D12GraphicsCommandList* pCmdList) const;
    std::size_t dupCnt() const NOEXCEPT { return internalResArr_[0].size(); }
    std::size_t frameIdx() const NOEXCEPT { return frameIdx_; }
    void setFrame(std::size_t frameIdx) NOEXCEPT( NDEBUG ) {
    #if !NDEBUG
        if (frameIdx >= dupCnt()) {
            throw std::out_of_range("Frame index out of range");
        }
    #endif
        frameIdx_ = frameIdx;
    }

    GpuMappedRes& pfd() NOEXCEPT { return resPerFrameData_; }
    GpuMappedRes& pdd() NOEXCEPT { return resPerDrawcallData_; }
    GpuMappedRes& pid() NOEXCEPT { return resPerInstanceData_; }
    GpuMappedRes& lights() NOEXCEPT { return resLights_; }

private:
    std::vector< std::vector< wrl::ComPtr<ID3D12Resource> > > internalResArr_;

    GpuMappedRes resPerFrameData_;
    GpuMappedRes resPerDrawcallData_;
    GpuMappedRes resPerInstanceData_;
    GpuMappedRes resLights_;

    std::size_t maxInstances_;
    std::size_t maxLights_;
    std::size_t frameIdx_;
};

class PhongShader : public Shader {
public:
    static constexpr std::size_t defMaxInstances = PhongShaderNT::defMaxInstances;
    static constexpr std::size_t defMaxLights = PhongShaderNT::defMaxLights;

    struct Config {
        size_t maxInstances = defMaxInstances;
        size_t maxLights = defMaxLights;
    };

    PhongShader(Core& core, const Config& config = Config{}, std::size_t duplicationCnt = 1u);

    static constexpr std::string shaderName() NOEXCEPT {
        return "PhongShader";
    }

    static Core::RootIdx rootName() NOEXCEPT {
        return d3d12::rootName(RootPreset::Unified1);
    }

    static Core::InputLayoutIdx inputLayoutName() NOEXCEPT {
        return d3d12::inputLayoutName(InputLayoutPreset::Pos3Norm3Tex2);
    }

    void setRootParams(ID3D12GraphicsCommandList* pCmdList, size_t frameIdx = 0) const;
    std::size_t dupCnt() const NOEXCEPT { return internalResArr_[0].size(); }
    std::size_t frameIdx() const NOEXCEPT { return frameIdx_; }
    void setFrame(std::size_t frameIdx) NOEXCEPT(NDEBUG) {
    #if !NDEBUG
        if (frameIdx >= dupCnt()) {
            throw std::out_of_range("Frame index out of range");
        }
    #endif
        frameIdx_ = frameIdx;
    }

    GpuMappedRes& pfd() NOEXCEPT { return resPerFrameData_; }
    GpuMappedRes& pdd() NOEXCEPT { return resPerDrawcallData_; }
    GpuMappedRes& pid() NOEXCEPT { return resPerInstanceData_; }
    GpuMappedRes& lights() NOEXCEPT { return resLights_; }
    D3D12_GPU_DESCRIPTOR_HANDLE texSrvStart() NOEXCEPT { return texSrvStart_; }

private:
    std::vector< std::vector< wrl::ComPtr<ID3D12Resource> > > internalResArr_;

    GpuMappedRes resPerFrameData_;
    GpuMappedRes resPerDrawcallData_;
    GpuMappedRes resPerInstanceData_;
    GpuMappedRes resLights_;
    D3D12_GPU_DESCRIPTOR_HANDLE texSrvStart_;
    DescriptorHeap* pTexSrvHeap_;   // temporary

    std::size_t maxInstances_;
    std::size_t maxLights_;
    std::size_t frameIdx_;
};


}	// namespace gfx::d3d12

}	// namespace gfx

#endif // !__PHONGSHADER_HPP
