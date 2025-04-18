#ifndef __renderer_HPP
#define __renderer_HPP

#include "d3d12engine/d3d12Engine.hpp"

class Renderer : public gfx::d3d12engine::IRenderer {
public:
    enum class Mode {
        Color,
        Albedo,
        Normal,
        Depth,
        DirectionalLightDepth,
    };

    Renderer(gfx::d3d12engine::Core& core);

    void init(gfx::d3d12engine::Scene& scene) override;
    void render( gfx::d3d12engine::Core& core, gfx::d3d12engine::Scene& scene,
        gfx::d3d12::RenderTargets& renderTargets
    ) override;
    void layoutVBsPBR( gfx::d3d12::D3D12Device& device,
        gfx::d3d12::D3D12GfxCmdList& cmdList,
        gfx::d3d12::RefModel& refModel,
        std::size_t layoutIdx
    );
    void layoutVBsPBRAnimated(gfx::d3d12::D3D12Device& device,
        gfx::d3d12::D3D12GfxCmdList& cmdList,
        gfx::d3d12::RefModel& refModel,
        std::size_t layoutIdx
    );
    void setMode(Mode renderMode) NOEXCEPT {
        renderMode_ = renderMode;
    }

private:
    static constexpr auto slotKeyTexture = "texture";

    gfx::d3d12::ResourceStorage rendererTexStorage_;

    gfx::d3d12::ShaderPBRIllumination shaderPBR_;
    gfx::d3d12engine::rp::PBRIllumination renderPassPBR_;

    gfx::d3d12::ShaderPBRAnimatedIllumination shaderPBRAnimated_;
    gfx::d3d12engine::rp::PBRAnimatedIllumination renderPassPBRAnimated_;

    gfx::d3d12::ShaderShadowMap shaderShadowMap_;
    gfx::d3d12engine::rp::ShadowMap renderPassShadowMap_;

    gfx::d3d12::ShaderScreenQuad shaderScreenQuad_;
    gfx::d3d12engine::rp::ScreenQuad renderPassScreenQuad_;

    gfx::d3d12::ShaderTessellation shaderTessellation_;
    gfx::d3d12engine::rp::Tessellation renderPassTessellation_;

    gfx::d3d12::ShaderShadowMapTessellation shaderShadowMapTessellation_;
    gfx::d3d12engine::rp::ShadowMapTessellation renderPassShadowMapTessellation_;

    Mode renderMode_;
};

class RenderModeController {
public:
    RenderModeController(Renderer* pRenderer) NOEXCEPT
        : pRenderer_(pRenderer) {}

    void setMode(Renderer::Mode renderMode) const NOEXCEPT {
        pRenderer_->setMode(renderMode);
    }

private:
    Renderer* pRenderer_;
};

#endif  // __renderer_HPP