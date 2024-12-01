#include "renderer.hpp"

Renderer::Renderer(gfx::d3d12engine::Core& core)
    : shader_( core.device(), core.root(),
        gfx::d3d12::ShaderPBRIllumination::Config{
            .maxInstanceCnt = 0x1000u,
            .maxDrawcallCnt = 0x1000u,
            .maxLightCnt = 0x100u
        }, gfx::d3d12::InputLayout::Spec::separated
    ), renderPass_( core.device(), shader_,
        gfx::d3d12::convClientToVP( core.window().client() )
    ) {}

void Renderer::layoutVBs( gfx::d3d12engine::Core& core,
    const gfx::d3d12::RefModelStorage::ID& key,
    std::size_t layoutIdx
) {
    core.layoutRefModelVBs( key, layoutIdx, shader_.inputLayout() );
}

void Renderer::init(gfx::d3d12engine::Scene& scene) {
    renderPass_.init(scene);
}

void Renderer::render(gfx::d3d12engine::Core& core, gfx::d3d12engine::Scene& scene) {
    renderPass_.update(scene);

    auto cmdList = core.fetchCmdList();

    shader_.bindRootParams( cmdList );
    renderPass_.preRender( cmdList );
    renderPass_.render( cmdList );
    renderPass_.postRender( cmdList );
}