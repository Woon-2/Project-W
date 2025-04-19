#include "renderer.hpp"

Renderer::Renderer(gfx::d3d12engine::Core& core)
    : rendererTexStorage_(),
    shaderPBR_( core.device(), core.root(),
        gfx::d3d12::ShaderPBRIllumination::Config{
            .maxInstanceCnt = 0x1000u,
            .maxDrawcallCnt = 0x1000u,
            .maxLightCnt = 0x100u
        }, gfx::d3d12::InputLayout::Spec::separated
    ), renderPassPBR_( core.device(), shaderPBR_, core.samStorage(),
        gfx::d3d12::convClientToVP( core.window().client() )
    ), shaderPBRAnimated_( core.device(), core.root(),
        gfx::d3d12::ShaderPBRAnimatedIllumination::Config{
            .maxInstanceCnt = 0x1000u,
            .maxDrawcallCnt = 0x1000u,
            .maxLightCnt = 0x100u,
            .maxBoneCnt = 10'000'000u
        }, gfx::d3d12::InputLayout::Spec::separated
    ), renderPassPBRAnimated_( core.device(), shaderPBRAnimated_,
        core.samStorage(), gfx::d3d12::convClientToVP( core.window().client() )
    ), shaderShadowMap_( core.device(), core.root(),
        gfx::d3d12::ShaderShadowMap::Config{
            .maxInstanceCnt = 0x1000u,
            .maxDrawcallCnt = 0x1000u
        }, gfx::d3d12::InputLayout::Spec::separated
    ), renderPassShadowMap_( core.device(), shaderShadowMap_,
        gfx::d3d12::convClientToVP( core.window().client() )
    ), shaderShadowMapAnimated_( core.device(), core.root(),
        gfx::d3d12::ShaderShadowMapAnimated::Config{
            .maxInstanceCnt = 0x1000u,
            .maxDrawcallCnt = 0x1000u,
            .maxBoneCnt = 10'000'000u
        }, gfx::d3d12::InputLayout::Spec::separated
    ), renderPassShadowMapAnimated_( core.device(), shaderShadowMapAnimated_,
        gfx::d3d12::convClientToVP( core.window().client() )
    ), shaderScreenQuad_(core.device(), core.root()),
    renderPassScreenQuad_( core.device(), shaderScreenQuad_,
        core.samStorage(), gfx::d3d12::convClientToVP(core.window().client())
    ), renderMode_(Mode::Color
    ), shaderTessellation_(core.device(), core.root(), 
        gfx::d3d12::ShaderTessellation::Config{
            .maxInstanceCnt = 0x1000u,
            .maxDrawcallCnt = 0x1000u,
            .maxLightCnt = 0x100u
        }, gfx::d3d12::InputLayout::Spec::serial
    ), renderPassTessellation_(core.device(), shaderTessellation_,
        core.samStorage(), gfx::d3d12::convClientToVP(core.window().client())
    ), shaderShadowMapTessellation_(core.device(), core.root(),
        gfx::d3d12::ShaderShadowMapTessellation::Config{
            .maxInstanceCnt = 0x1000u,
            .maxDrawcallCnt = 0x1000u,
        }, gfx::d3d12::InputLayout::Spec::serial
    ), renderPassShadowMapTessellation_(core.device(),
        shaderShadowMapTessellation_, core.samStorage(),
        gfx::d3d12::convClientToVP(core.window().client())
    ) {
    rendererTexStorage_.addSlot(
        slotKeyTexture,
        gfx::d3d12::ResourceStorage::ResType::Texture
    );

    auto [pTex, srvDesc, dsvDesc] = gfx::d3d12::loadShadowMapAt(
        rendererTexStorage_.slot(slotKeyTexture),
        "shadowMap",
        core.device(), core.descRanges().srvRangeTex2D, core.descRanges().dsvRange,
        gfx::d3d12::Texture::Desc{
            .width = static_cast<std::uint32_t>(core.window().client().width * 8),
            .height = static_cast<std::uint32_t>(core.window().client().height * 8),
            .mipLevels = 1u,
            .format = DXGI_FORMAT_D32_FLOAT,
            .sampleDesc = { .Count = 1u, .Quality = 0u },
            .flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
        }
    );

    renderPassPBR_.mapTexture(gfx::d3d12::RenderPassTextures::ShadowMap, pTex);
    renderPassPBR_.initResources(
        gfx::d3d12::RenderPassTextures::ShadowMap, &pTex->view(1u),
        dsvDesc, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pTex->view(0u)), srvDesc
    );

    renderPassPBRAnimated_.mapTexture(gfx::d3d12::RenderPassTextures::ShadowMap, pTex);
    renderPassPBRAnimated_.initResources(
        gfx::d3d12::RenderPassTextures::ShadowMap, &pTex->view(1u),
        dsvDesc, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pTex->view(0u)), srvDesc
    );

    renderPassShadowMap_.mapTexture(gfx::d3d12::RenderPassTextures::ShadowMap, pTex);
    renderPassShadowMap_.initResources(
        gfx::d3d12::RenderPassTextures::ShadowMap, &pTex->view(1u),
        dsvDesc, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pTex->view(0u)), srvDesc
    );

    renderPassShadowMapAnimated_.mapTexture(gfx::d3d12::RenderPassTextures::ShadowMap, pTex);
    renderPassShadowMapAnimated_.initResources(
        gfx::d3d12::RenderPassTextures::ShadowMap, &pTex->view(1u),
        dsvDesc, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pTex->view(0u)), srvDesc
    );

    renderPassTessellation_.mapTexture(gfx::d3d12::RenderPassTextures::ShadowMap, pTex);
    renderPassTessellation_.initResources(
        gfx::d3d12::RenderPassTextures::ShadowMap, &pTex->view(1u),
        dsvDesc, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pTex->view(0u)), srvDesc
    );

    renderPassShadowMapTessellation_.mapTexture(gfx::d3d12::RenderPassTextures::ShadowMap, pTex);
    renderPassShadowMapTessellation_.initResources(
        gfx::d3d12::RenderPassTextures::ShadowMap, &pTex->view(1u),
        dsvDesc, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pTex->view(0u)), srvDesc
    );
}

void Renderer::layoutVBsPBR( gfx::d3d12::D3D12Device& device,
    gfx::d3d12::D3D12GfxCmdList& cmdList,
    gfx::d3d12::RefModel& refModel,
    std::size_t layoutIdx
) {
    gfx::d3d12::arrangeVBs(refModel, device, cmdList, layoutIdx,
        shaderPBR_.inputLayout()
    );
}

void Renderer::layoutVBsPBRAnimated(gfx::d3d12::D3D12Device& device,
    gfx::d3d12::D3D12GfxCmdList& cmdList,
    gfx::d3d12::RefModel& refModel,
    std::size_t layoutIdx
) {
    gfx::d3d12::arrangeVBs(refModel, device, cmdList, layoutIdx,
        shaderPBRAnimated_.inputLayout()
    );
    gfx::d3d12::arrangeVBs(refModel, device, cmdList, layoutIdx + 1u,
        shaderShadowMapAnimated_.inputLayout()
    );
}

void Renderer::init(gfx::d3d12engine::Scene& scene) {
    renderPassPBR_.init(scene);
    renderPassPBRAnimated_.init(scene);
    renderPassShadowMap_.init(scene);
    renderPassShadowMapAnimated_.init(scene);
    renderPassScreenQuad_.init(scene);
    renderPassTessellation_.init(scene);
    renderPassShadowMapTessellation_.init(scene);
}

void Renderer::render(gfx::d3d12engine::Core& core, gfx::d3d12engine::Scene& scene, gfx::d3d12::RenderTargets& renderTargets) {
    auto cmdList = core.fetchCmdList();

    renderPassPBR_.update(scene);
    renderPassPBRAnimated_.update(scene);
    renderPassShadowMap_.update(scene);
    renderPassShadowMapAnimated_.update(scene);
    renderPassScreenQuad_.update(scene);
    renderPassTessellation_.update(scene);
    renderPassShadowMapTessellation_.update(scene);

    switch (renderMode_) {
    case Mode::Color:
        cmdList.get()->ClearDepthStencilView(
            rendererTexStorage_.slot(slotKeyTexture).get<gfx::d3d12::Texture>("shadowMap")->view(1u).cpuHandle(),
            D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0u, 0u, nullptr   
        );

        shaderShadowMapAnimated_.bindRootParams( cmdList );
        renderPassShadowMapAnimated_.preRender( cmdList, renderTargets );
        renderPassShadowMapAnimated_.render( cmdList, renderTargets );
        renderPassShadowMapAnimated_.postRender( cmdList, renderTargets );

        shaderShadowMap_.bindRootParams( cmdList );
        renderPassShadowMap_.preRender( cmdList, renderTargets );
        renderPassShadowMap_.render( cmdList, renderTargets );
        renderPassShadowMap_.postRender( cmdList, renderTargets );

        shaderShadowMapTessellation_.bindRootParams( cmdList );
        renderPassShadowMapTessellation_.preRender( cmdList, renderTargets );
        renderPassShadowMapTessellation_.render( cmdList, renderTargets );
        renderPassShadowMapTessellation_.postRender( cmdList, renderTargets );

        shaderPBRAnimated_.bindRootParams( cmdList );
        renderPassPBRAnimated_.preRender( cmdList, renderTargets );
        renderPassPBRAnimated_.render( cmdList, renderTargets );
        renderPassPBRAnimated_.postRender( cmdList, renderTargets );

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
        cmdList.get()->ClearDepthStencilView(
            rendererTexStorage_.slot(slotKeyTexture).get<gfx::d3d12::Texture>("shadowMap")->view(1u).cpuHandle(),
            D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0u, 0u, nullptr   
        );

        shaderShadowMap_.bindRootParams( cmdList );
        renderPassShadowMap_.preRender( cmdList, renderTargets );
        renderPassShadowMap_.render( cmdList, renderTargets );
        renderPassShadowMap_.postRender( cmdList, renderTargets );

        shaderShadowMapTessellation_.bindRootParams( cmdList );
        renderPassShadowMapTessellation_.preRender( cmdList, renderTargets );
        renderPassShadowMapTessellation_.render( cmdList, renderTargets );
        renderPassShadowMapTessellation_.postRender( cmdList, renderTargets );

        shaderScreenQuad_.bindRootParams( cmdList );
        shaderScreenQuad_.screenQuad_.link(rendererTexStorage_.slot(slotKeyTexture).get<gfx::d3d12::Texture>("shadowMap"));
        renderPassScreenQuad_.preRender( cmdList, renderTargets );
        renderPassScreenQuad_.render( cmdList, renderTargets );
        renderPassScreenQuad_.postRender( cmdList, renderTargets );
        break;

    default:
        throw GFX_EXCEPT("[Description] Unknown render mode");
    }
}