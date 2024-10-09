#ifndef __ShadowShader_HPP
#define __ShadowShader_HPP

#include "d3d12shader.hpp"
#include "d3d12res.hpp"
#include "d3d12texture.hpp"

#include "shaderPath.hpp"
#include "rootPresets.hpp"
#include "d3d12InputLayoutPresets.hpp"

#include <vector>
#include <cstdint>

namespace gfx {

namespace d3d12 {

class ShadowShader : public Shader {
public:
    static constexpr std::size_t defMaxInstCnt = 1000u;

    struct Config {
        std::uint32_t shadowMapWidth;
        std::uint32_t shadowMapHeight;
        std::size_t maxInstCnt = defMaxInstCnt;
    };

    ShadowShader(Core& core, const Config& config = Config{}, std::size_t duplicationCnt = 1u);

    static constexpr std::string shaderName() NOEXCEPT {
        return "ShadowShader";
    }

    static Core::RootIdx rootName() NOEXCEPT {
        return d3d12::rootName(RootPreset::Unified1);
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
    D3D12_GPU_DESCRIPTOR_HANDLE srvHeapStart() NOEXCEPT { return srvHeapStart_; }
    
    std::size_t maxInstCnt() const NOEXCEPT { return maxInstances_; }

    void draw( IRenderContext& ctx, const IScene& scene,
        IRenderTarget& target, rp::Protocol protocol
    ) override;

    BindOption optGeneral() const NOEXCEPT {
        return BindOption{ .idx = 0, .ilIdx = 0 };
    }

    Texture& shadowMap() NOEXCEPT { return shadowMap_; }

private:
    void preDraw( IRenderContext& ctx, const IScene& scene,
        IRenderTarget& target, rp::Protocol protocol
    );
    void drawImpl( D3D12RenderContext& ctx, const IScene& scene,
        rp::Protocol protocol
    );

    Texture shadowMap_;
    std::vector< std::vector< wrl::ComPtr<ID3D12Resource> > > internalResArr_;
    GpuMappedRes resPerFrameData_;
    GpuMappedRes resPerDrawcallData_;
    GpuMappedRes resPerInstanceData_;
    D3D12_GPU_DESCRIPTOR_HANDLE srvHeapStart_;
    std::size_t maxInstances_;
    std::size_t frameIdx_;
};

}   // namespace gfx::d3d12

}   // namespace gfx

#endif // __ShadowShader_HPP