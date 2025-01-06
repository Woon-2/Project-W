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
    ), shaderPBRTerrain_( core.device(), core.root(),
        gfx::d3d12::ShaderPBRIlluminationTerrain::Config{
            .maxInstanceCnt = 0x1000u,
            .maxDrawcallCnt = 0x1000u,
            .maxLightCnt = 0x100u
        }, gfx::d3d12::InputLayout::Spec::separated
    ), renderPassPBRTerrain_( core.device(), shaderPBRTerrain_,
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
    core.layoutRefModelVBs( key, layoutIdx, shaderPBRTerrain_.inputLayout() );
}

void Renderer::init(gfx::d3d12engine::Scene& scene) {
    renderPassPBR_.init(scene);
    renderPassPBRTerrain_.init(scene);
}

void Renderer::render(gfx::d3d12engine::Core& core, gfx::d3d12engine::Scene& scene, gfx::d3d12::DescriptorCPU& rtv, gfx::d3d12::DescriptorCPU& dsv) {
    renderPassPBR_.update(scene);
    renderPassPBRTerrain_.update(scene);

    auto cmdList = core.fetchCmdList();

    shaderPBR_.bindRootParams( cmdList );
    renderPassPBR_.preRender( cmdList, rtv, dsv );
    renderPassPBR_.render( cmdList, rtv, dsv );
    renderPassPBR_.postRender( cmdList, rtv, dsv );

    shaderPBRTerrain_.bindRootParams( cmdList );
    renderPassPBRTerrain_.preRender( cmdList, rtv, dsv );
    renderPassPBRTerrain_.render( cmdList, rtv, dsv );
    renderPassPBRTerrain_.postRender( cmdList, rtv, dsv );
}