#include "renderer.hpp"

Renderer::Renderer(gfx::d3d12engine::Core& core)
    : shader_( core.device(), core.root(),
        gfx::d3d12::ShaderPBRIllumination::Config{
            .maxInstanceCnt = 0x1000u,
            .maxDrawcallCnt = 0x1000u,
            .maxLightCnt = 0x100u
        }
    ), renderPass_( core.device(), shader_,
        gfx::d3d12::convClientToVP( core.window().client() )
    ) {}