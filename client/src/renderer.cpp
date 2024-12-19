#include "renderer.hpp"

Renderer::Renderer(gfx::d3d12engine::Core& core)
    : shaderPBR_( core.device(), core.root(),
        gfx::d3d12::ShaderPBRIllumination::Config{
            .maxInstanceCnt = 0x1000u,
            .maxDrawcallCnt = 0x1000u,
            .maxLightCnt = 0x100u
        }, gfx::d3d12::InputLayout::Spec::separated
    ), renderPassPBR_( core.device(), shaderPBR_,
        gfx::d3d12::convClientToVP( core.window().client() )
    ), shaderPBRMacro_( core.device(), core.root(),
        gfx::d3d12::ShaderPBRIlluminationMacro::Config{
            .maxInstanceCnt = 0x1000u,
            .maxDrawcallCnt = 0x1000u,
            .maxLightCnt = 0x100u
        }, gfx::d3d12::InputLayout::Spec::separated
    ), renderPassPBRMacro_( core.device(), shaderPBRMacro_,
        gfx::d3d12::convClientToVP( core.window().client() )
    ) {}

void Renderer::layoutVBsPBR( gfx::d3d12engine::Core& core,
    const gfx::d3d12::RefModelStorage::ID& key,
    std::size_t layoutIdx
) {
    core.layoutRefModelVBs( key, layoutIdx, shaderPBR_.inputLayout() );
}

void Renderer::layoutVBsPBRMacro( gfx::d3d12engine::Core& core,
    const gfx::d3d12::RefModelStorage::ID& key,
    std::size_t layoutIdx
) {
    core.layoutRefModelVBs( key, layoutIdx, shaderPBRMacro_.inputLayout() );
}

void Renderer::init(gfx::d3d12engine::Scene& scene) {
    renderPassPBR_.init(scene);
    renderPassPBRMacro_.init(scene);
}

void Renderer::render(gfx::d3d12engine::Core& core, gfx::d3d12engine::Scene& scene) {
    renderPassPBR_.update(scene);
    renderPassPBRMacro_.update(scene);

    auto cmdList = core.fetchCmdList();

    shaderPBR_.bindRootParams( cmdList );
    renderPassPBR_.preRender( cmdList );
    renderPassPBR_.render( cmdList );
    renderPassPBR_.postRender( cmdList );

    shaderPBRMacro_.bindRootParams( cmdList );
    renderPassPBRMacro_.preRender( cmdList );
    renderPassPBRMacro_.render( cmdList );
    renderPassPBRMacro_.postRender( cmdList );
}