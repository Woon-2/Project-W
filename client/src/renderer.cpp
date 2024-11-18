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

void Renderer::render(gfx::d3d12engine::Core& core) {
    auto cmdList = core.fetchCmdList();

    shader_.bindRootParams( core.root(), cmdList );

    renderPass_.preRender( cmdList );
    renderPass_.render( cmdList );
    renderPass_.postRender( cmdList );
}