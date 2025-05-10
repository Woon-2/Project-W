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
    ), shaderCascadeShaodwMap_( core.device(), core.root(),
        gfx::d3d12::ShaderCascadeShadowMap::Config{
			.maxInstanceCnt = 0x1000u,
			.maxDrawcallCnt = 0x1000u,
		}, gfx::d3d12::InputLayout::Spec::separated
		), renderPassCascadeShadowMap_(core.device(), shaderCascadeShaodwMap_,
            gfx::d3d12::convClientToVP(core.window().client())
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
    rendererTexStorage_.addSlot(
        slotKeyTextureArray,
        gfx::d3d12::ResourceStorage::ResType::TexArray
    );

    auto [pTexArr, srvDesc, dsvDesc] = gfx::d3d12::loadShadowArrayMapAt(
        rendererTexStorage_.slot(slotKeyTextureArray),
        "shadowArray",    //
        core.device(), core.descRanges().srvRangeTex2DArray, core.descRanges().dsvRange,
        gfx::d3d12::TextureArray::Desc{
            .width = static_cast<std::uint32_t>(core.window().client().width),      //
            .height = static_cast<std::uint32_t>(core.window().client().height),    //
			.arraySize = 3u,    //
            .mipLevels = 1u,
            .format = DXGI_FORMAT_D32_FLOAT,
            .sampleDesc = {.Count = 1u, .Quality = 0u },
            .flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
        }
    );

    // Cascade Shadow Map
    renderPassCascadeShadowMap_.mapTextureArray(gfx::d3d12::RenderPassTextureArrays::ShadowMapArray, pTexArr);
    renderPassCascadeShadowMap_.initResources(
        gfx::d3d12::RenderPassTextureArrays::ShadowMapArray, &pTexArr->view(1u),
        dsvDesc, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pTexArr->view(0u)), srvDesc
    );

    // PBR
    renderPassPBR_.mapTextureArray(gfx::d3d12::RenderPassTextureArrays::ShadowMapArray, pTexArr);
    renderPassPBR_.initResources(
        gfx::d3d12::RenderPassTextureArrays::ShadowMapArray, &pTexArr->view(1u),
        dsvDesc, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pTexArr->view(0u)), srvDesc
    );

    // Animated PBR
    renderPassPBRAnimated_.mapTextureArray(gfx::d3d12::RenderPassTextureArrays::ShadowMapArray, pTexArr);
    renderPassPBRAnimated_.initResources(
        gfx::d3d12::RenderPassTextureArrays::ShadowMapArray, &pTexArr->view(1u),
        dsvDesc, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pTexArr->view(0u)), srvDesc
    );

	// Tessellation
    renderPassTessellation_.mapTextureArray(gfx::d3d12::RenderPassTextureArrays::ShadowMapArray, pTexArr);
    renderPassTessellation_.initResources(
        gfx::d3d12::RenderPassTextureArrays::ShadowMapArray, &pTexArr->view(1u),
        dsvDesc, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pTexArr->view(0u)), srvDesc
    );

	// Shadow Map Tessellation
    renderPassShadowMapTessellation_.mapTextureArray(gfx::d3d12::RenderPassTextureArrays::ShadowMapArray, pTexArr);
    renderPassShadowMapTessellation_.initResources(
        gfx::d3d12::RenderPassTextureArrays::ShadowMapArray, &pTexArr->view(1u),
        dsvDesc, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pTexArr->view(0u)), srvDesc
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
}

void Renderer::init(gfx::d3d12engine::Scene& scene) {
    renderPassPBR_.init(scene);
    renderPassPBRAnimated_.init(scene);
    renderPassScreenQuad_.init(scene);
    renderPassTessellation_.init(scene);
    renderPassShadowMapTessellation_.init(scene);
	renderPassCascadeShadowMap_.init(scene);
}

void Renderer::render(gfx::d3d12engine::Core& core, gfx::d3d12engine::Scene& scene, gfx::d3d12::RenderTargets& renderTargets) {
    auto cmdList = core.fetchCmdList();

    renderPassPBR_.update(scene);
    renderPassPBRAnimated_.update(scene);
	renderPassCascadeShadowMap_.update(scene);
    renderPassScreenQuad_.update(scene);
    renderPassTessellation_.update(scene);
    renderPassShadowMapTessellation_.update(scene);

    switch (renderMode_) {
    case Mode::Color:
        cmdList.get()->ClearDepthStencilView(
            rendererTexStorage_.slot(slotKeyTextureArray).get<gfx::d3d12::TextureArray>("shadowArray")->view(1u).cpuHandle(),
            D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0u, 0u, nullptr
        );

        shaderCascadeShaodwMap_.bindRootParams(cmdList);
        renderPassCascadeShadowMap_.preRender(cmdList, renderTargets);
        renderPassCascadeShadowMap_.render(cmdList, renderTargets);
        renderPassCascadeShadowMap_.postRender(cmdList, renderTargets);

        shaderShadowMapTessellation_.bindRootParams(cmdList);
        renderPassShadowMapTessellation_.preRender(cmdList, renderTargets);
        renderPassShadowMapTessellation_.render(cmdList, renderTargets);
        renderPassShadowMapTessellation_.postRender(cmdList, renderTargets);
  
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

    case Mode::Cascade0Depth:
        cmdList.get()->ClearDepthStencilView(
            rendererTexStorage_.slot(slotKeyTextureArray).get<gfx::d3d12::TextureArray>("shadowArray")->view(1u).cpuHandle(),
            D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0u, 0u, nullptr
        );

        shaderCascadeShaodwMap_.bindRootParams(cmdList);
        renderPassCascadeShadowMap_.preRender(cmdList, renderTargets);
        renderPassCascadeShadowMap_.render(cmdList, renderTargets);
        renderPassCascadeShadowMap_.postRender(cmdList, renderTargets);

        shaderShadowMapTessellation_.bindRootParams(cmdList);
        renderPassShadowMapTessellation_.preRender(cmdList, renderTargets);
        renderPassShadowMapTessellation_.render(cmdList, renderTargets);
        renderPassShadowMapTessellation_.postRender(cmdList, renderTargets);

        shaderScreenQuad_.bindRootParams( cmdList );
        shaderScreenQuad_.screenQuad_.link(rendererTexStorage_.slot(slotKeyTexture).get<gfx::d3d12::Texture>("shadowArray"));
        renderPassScreenQuad_.preRender( cmdList, renderTargets );
        renderPassScreenQuad_.render( cmdList, renderTargets );
        renderPassScreenQuad_.postRender( cmdList, renderTargets );
        break;

    case Mode::Cascade1Depth:
        cmdList.get()->ClearDepthStencilView(
            rendererTexStorage_.slot(slotKeyTextureArray).get<gfx::d3d12::TextureArray>("shadowArray")->view(1u).cpuHandle(),
            D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0u, 0u, nullptr
        );

        shaderCascadeShaodwMap_.bindRootParams(cmdList);
        renderPassCascadeShadowMap_.preRender(cmdList, renderTargets);
        renderPassCascadeShadowMap_.render(cmdList, renderTargets);
        renderPassCascadeShadowMap_.postRender(cmdList, renderTargets);

        shaderShadowMapTessellation_.bindRootParams(cmdList);
        renderPassShadowMapTessellation_.preRender(cmdList, renderTargets);
        renderPassShadowMapTessellation_.render(cmdList, renderTargets);
        renderPassShadowMapTessellation_.postRender(cmdList, renderTargets);

        shaderScreenQuad_.bindRootParams(cmdList);
        shaderScreenQuad_.screenQuad_.link(rendererTexStorage_.slot(slotKeyTexture).get<gfx::d3d12::Texture>("TextureArray"));
        renderPassScreenQuad_.preRender(cmdList, renderTargets);
        renderPassScreenQuad_.render(cmdList, renderTargets);
        renderPassScreenQuad_.postRender(cmdList, renderTargets);
        break;

    case Mode::Cascade2Depth:
        cmdList.get()->ClearDepthStencilView(
            rendererTexStorage_.slot(slotKeyTextureArray).get<gfx::d3d12::TextureArray>("shadowArray")->view(1u).cpuHandle(),
            D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0u, 0u, nullptr
        );

        shaderCascadeShaodwMap_.bindRootParams(cmdList);
        renderPassCascadeShadowMap_.preRender(cmdList, renderTargets);
        renderPassCascadeShadowMap_.render(cmdList, renderTargets);
        renderPassCascadeShadowMap_.postRender(cmdList, renderTargets);

        shaderShadowMapTessellation_.bindRootParams(cmdList);
        renderPassShadowMapTessellation_.preRender(cmdList, renderTargets);
        renderPassShadowMapTessellation_.render(cmdList, renderTargets);
        renderPassShadowMapTessellation_.postRender(cmdList, renderTargets);

        shaderScreenQuad_.bindRootParams(cmdList);
        shaderScreenQuad_.screenQuad_.link(rendererTexStorage_.slot(slotKeyTexture).get<gfx::d3d12::Texture>("TextureArray"));
        renderPassScreenQuad_.preRender(cmdList, renderTargets);
        renderPassScreenQuad_.render(cmdList, renderTargets);
        renderPassScreenQuad_.postRender(cmdList, renderTargets);
        break;

    default:
        throw GFX_EXCEPT("[Description] Unknown render mode");
    }
}