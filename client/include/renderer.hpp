#ifndef __renderer_HPP
#define __renderer_HPP

#include "d3d12engine/d3d12Engine.hpp"

class Renderer : public gfx::d3d12engine::IRenderer {
public:
    Renderer(gfx::d3d12engine::Core& core);

    void init(gfx::d3d12engine::Scene& scene) override;
    void render(gfx::d3d12engine::Core& core, gfx::d3d12engine::Scene& scene, gfx::d3d12::RenderTargets& renderTargets) override;
    void layoutVBsPBR( gfx::d3d12engine::Core& core,
        const gfx::d3d12::RefModelStorage::ID& key,
        std::size_t layoutIdx
    );
    void layoutVBsPBRMacro( gfx::d3d12engine::Core& core,
        const gfx::d3d12::RefModelStorage::ID& key,
        std::size_t layoutIdx
    );

private:
    gfx::d3d12::ShaderPBRIllumination shaderPBR_;
    gfx::d3d12engine::rp::PBRIllumination renderPassPBR_;

    gfx::d3d12::ShaderPBRIlluminationTerrain shaderPBRTerrain_;
    gfx::d3d12engine::rp::PBRIlluminationTerrain renderPassPBRTerrain_;

    gfx::d3d12::ShaderShadowMap shaderShadowMap_;
    gfx::d3d12engine::rp::ShadowMap renderPassShadowMap_;
};

#endif  // __renderer_HPP