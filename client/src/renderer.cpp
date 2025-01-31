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
    ), shadowMaterial_( core.device(), gfx::d3d12::Texture::Desc{
            .width = static_cast<std::uint32_t>( core.window().client().width ),
            .height = static_cast<std::uint32_t>( core.window().client().height ),
            .mipLevels = 1u,
            .format = DXGI_FORMAT_D32_FLOAT,
            .sampleDesc = { .Count = 1u, .Quality = 0u },
            .flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
        }, core.descRanges().srvRangeTex2D, core.descRanges().dsvRange
    ), shaderShadowMap_( core.device(), core.root(),
        gfx::d3d12::ShaderShadowMap::Config{
            .maxInstanceCnt = 0x1000u,
            .maxDrawcallCnt = 0x1000u
        }, gfx::d3d12::InputLayout::Spec::separated
    ), renderPassShadowMap_( core.device(), shaderShadowMap_,
        shadowMaterial_, shadowMaterial_.idxDsv,
        gfx::d3d12::convClientToVP( core.window().client() )
    ), shaderScreenQuad_(core.device(), core.root()),
    renderPassScreenQuad_( core.device(), shaderScreenQuad_,
        gfx::d3d12::convClientToVP(core.window().client())
    ), renderMode_(Mode::Color
    ), shaderTessellation_(core.device(), core.root(), 
        gfx::d3d12::ShaderTessellation::Config{
            .maxInstanceCnt = 0x1000u,
            .maxDrawcallCnt = 0x1000u,
            .maxLightCnt = 0x100u
        }, gfx::d3d12::InputLayout::Spec::serial
    ), renderPassTessellation_(core.device(), shaderTessellation_,
        gfx::d3d12::convClientToVP(core.window().client())
    ), shaderShadowMapTessellation_(core.device(), core.root(),
        gfx::d3d12::ShaderShadowMapTessellation::Config{
            .maxInstanceCnt = 0x1000u,
            .maxDrawcallCnt = 0x1000u,
        }, gfx::d3d12::InputLayout::Spec::serial
    ), renderPassShadowMapTessellation_(core.device(),
        shaderShadowMapTessellation_, renderPassShadowMap_
    ) {
        shaderShadowMap_.setShadowMap( &shadowMaterial_.texture() );
        shaderShadowMapTessellation_.setShadowMap( &shadowMaterial_.texture() );
    }

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
    renderPassShadowMap_.init(scene);
    renderPassTessellation_.init(scene);
    renderPassShadowMapTessellation_.init(scene);
}

void Renderer::render(gfx::d3d12engine::Core& core, gfx::d3d12engine::Scene& scene, gfx::d3d12::RenderTargets& renderTargets) {
    auto cmdList = core.fetchCmdList();

    renderPassPBR_.update(scene);
    renderPassPBRTerrain_.update(scene);
    renderPassShadowMap_.update(scene);
    renderPassScreenQuad_.update(scene);
    renderPassTessellation_.update(scene);
    renderPassShadowMapTessellation_.update(scene);

    switch (renderMode_) {
    case Mode::Color:
        shaderShadowMap_.bindRootParams( cmdList );
        renderPassShadowMap_.preRender( cmdList, renderTargets );
        renderPassShadowMap_.render( cmdList, renderTargets );
        renderPassShadowMap_.postRender( cmdList, renderTargets );

        shaderShadowMapTessellation_.bindRootParams( cmdList );
        renderPassShadowMapTessellation_.preRender( cmdList, renderTargets );
        renderPassShadowMapTessellation_.render( cmdList, renderTargets );
        renderPassShadowMapTessellation_.postRender( cmdList, renderTargets );

        shaderPBRTerrain_.bindRootParams( cmdList );
        renderPassPBRTerrain_.preRender( cmdList, renderTargets );
        renderPassPBRTerrain_.render( cmdList, renderTargets );
        renderPassPBRTerrain_.postRender( cmdList, renderTargets );

        shaderPBR_.bindRootParams( cmdList );
        renderPassPBR_.preRender( cmdList, renderTargets );
        renderPassPBR_.render( cmdList, renderTargets );
        renderPassPBR_.postRender( cmdList, renderTargets );

        shaderTessellation_.bindRootParams( cmdList );
        renderPassTessellation_.preRender( cmdList, renderTargets );
        renderPassTessellation_.render( cmdList, renderTargets );
        renderPassTessellation_.postRender( cmdList, renderTargets );
        break;

    case Mode::DirectionalLightDepth:
        shaderShadowMap_.bindRootParams( cmdList );
        renderPassShadowMap_.preRender( cmdList, renderTargets );
        renderPassShadowMap_.render( cmdList, renderTargets );
        renderPassShadowMap_.postRender( cmdList, renderTargets );

        shaderShadowMapTessellation_.bindRootParams( cmdList );
        renderPassShadowMapTessellation_.preRender( cmdList, renderTargets );
        renderPassShadowMapTessellation_.render( cmdList, renderTargets );
        renderPassShadowMapTessellation_.postRender( cmdList, renderTargets );

        shaderScreenQuad_.bindRootParams( cmdList );
        shaderScreenQuad_.screenQuad_.link(shaderShadowMap_.shadowMap());
        renderPassScreenQuad_.preRender( cmdList, renderTargets );
        renderPassScreenQuad_.render( cmdList, renderTargets );
        renderPassScreenQuad_.postRender( cmdList, renderTargets );
        break;

    default:
        throw GFX_EXCEPT("[Description] Unknown render mode");
    }
}