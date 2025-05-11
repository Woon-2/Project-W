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
    ), shaderCascadeShadowMapAnimated_( core.device(), core.root(),
        gfx::d3d12::ShaderCascadeShadowMapAnimated::Config{
            .maxInstanceCnt = 0x1000u,
            .maxDrawcallCnt = 0x1000u,
        }, gfx::d3d12::InputLayout::Spec::separated
    ), renderPassCascadeShadowMapAnimated_( core.device(),
        shaderCascadeShadowMapAnimated_,
        gfx::d3d12::convClientToVP(core.window().client())
    ),
    shaderScreenQuad_(core.device(), core.root()),
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

    auto [pCascade0, srvDescC0, dsvDescC0] = gfx::d3d12::loadShadowMapAt(
        rendererTexStorage_.slot(slotKeyTexture),
        "cascade0",    //
        core.device(), core.descRanges().srvRangeTex2D, core.descRanges().dsvRange,
        gfx::d3d12::Texture::Desc{
            .width = static_cast<std::uint32_t>(core.window().client().width),      //
            .height = static_cast<std::uint32_t>(core.window().client().height),    //
            .mipLevels = 1u,
            .format = DXGI_FORMAT_D32_FLOAT,
            .sampleDesc = {.Count = 1u, .Quality = 0u },
            .flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
        }
    );

    auto [pCascade1, srvDescC1, dsvDescC1] = gfx::d3d12::loadShadowMapAt(
        rendererTexStorage_.slot(slotKeyTexture),
        "cascade1",    //
        core.device(), core.descRanges().srvRangeTex2D, core.descRanges().dsvRange,
        gfx::d3d12::Texture::Desc{
            .width = static_cast<std::uint32_t>(core.window().client().width),      //
            .height = static_cast<std::uint32_t>(core.window().client().height),    //
            .mipLevels = 1u,
            .format = DXGI_FORMAT_D32_FLOAT,
            .sampleDesc = {.Count = 1u, .Quality = 0u },
            .flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
        }
    );
    
    auto [pCascade2, srvDescC2, dsvDescC2] = gfx::d3d12::loadShadowMapAt(
        rendererTexStorage_.slot(slotKeyTexture),
        "cascade2",    //
        core.device(), core.descRanges().srvRangeTex2D, core.descRanges().dsvRange,
        gfx::d3d12::Texture::Desc{
            .width = static_cast<std::uint32_t>(core.window().client().width),      //
            .height = static_cast<std::uint32_t>(core.window().client().height),    //
            .mipLevels = 1u,
            .format = DXGI_FORMAT_D32_FLOAT,
            .sampleDesc = {.Count = 1u, .Quality = 0u },
            .flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
        }
    );

    // Cascade Shadow Map
    renderPassCascadeShadowMap_.mapTexture(gfx::d3d12::RenderPassTextures::ShadowCascade0, pCascade0);
    renderPassCascadeShadowMap_.mapTexture(gfx::d3d12::RenderPassTextures::ShadowCascade1, pCascade1);
    renderPassCascadeShadowMap_.mapTexture(gfx::d3d12::RenderPassTextures::ShadowCascade2, pCascade2);
    renderPassCascadeShadowMap_.initResources(
        0, gfx::d3d12::RenderPassTextures::ShadowCascade0, &pCascade0->view(1u),
        dsvDescC0, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pCascade0->view(0u)), srvDescC0
    );
    renderPassCascadeShadowMap_.initResources(
        1, gfx::d3d12::RenderPassTextures::ShadowCascade1, &pCascade1->view(1u),
        dsvDescC1, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pCascade1->view(0u)), srvDescC1
    );
    renderPassCascadeShadowMap_.initResources(
        2, gfx::d3d12::RenderPassTextures::ShadowCascade2, &pCascade2->view(1u),
        dsvDescC2, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pCascade2->view(0u)), srvDescC2
    );

    // Animated Cascade Shadow Map
    renderPassCascadeShadowMapAnimated_.mapTexture(gfx::d3d12::RenderPassTextures::ShadowCascade0, pCascade0);
    renderPassCascadeShadowMapAnimated_.mapTexture(gfx::d3d12::RenderPassTextures::ShadowCascade1, pCascade1);
    renderPassCascadeShadowMapAnimated_.mapTexture(gfx::d3d12::RenderPassTextures::ShadowCascade2, pCascade2);
    renderPassCascadeShadowMapAnimated_.initResources(
        0, gfx::d3d12::RenderPassTextures::ShadowCascade0, &pCascade0->view(1u),
        dsvDescC0, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pCascade0->view(0u)), srvDescC0
    );
    renderPassCascadeShadowMapAnimated_.initResources(
        1, gfx::d3d12::RenderPassTextures::ShadowCascade1, &pCascade1->view(1u),
        dsvDescC1, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pCascade1->view(0u)), srvDescC1
    );
    renderPassCascadeShadowMapAnimated_.initResources(
        2, gfx::d3d12::RenderPassTextures::ShadowCascade2, &pCascade2->view(1u),
        dsvDescC2, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pCascade2->view(0u)), srvDescC2
    );

    // PBR
    renderPassPBR_.mapTexture(gfx::d3d12::RenderPassTextures::ShadowCascade0, pCascade0);
    renderPassPBR_.mapTexture(gfx::d3d12::RenderPassTextures::ShadowCascade1, pCascade1);
    renderPassPBR_.mapTexture(gfx::d3d12::RenderPassTextures::ShadowCascade2, pCascade2);
    renderPassPBR_.initResources(
        0, gfx::d3d12::RenderPassTextures::ShadowCascade0, &pCascade0->view(1u),
        dsvDescC0, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pCascade0->view(0u)), srvDescC0
    );
	renderPassPBR_.initResources(
		1, gfx::d3d12::RenderPassTextures::ShadowCascade1, &pCascade1->view(1u),
		dsvDescC1, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pCascade1->view(0u)), srvDescC1
	);
    renderPassPBR_.initResources(
        2, gfx::d3d12::RenderPassTextures::ShadowCascade2, &pCascade1->view(1u),
        dsvDescC2, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pCascade2->view(0u)), srvDescC2
    );

    // Animated PBR
    renderPassPBRAnimated_.mapTexture(gfx::d3d12::RenderPassTextures::ShadowCascade0, pCascade0);
	renderPassPBRAnimated_.mapTexture(gfx::d3d12::RenderPassTextures::ShadowCascade1, pCascade1);
	renderPassPBRAnimated_.mapTexture(gfx::d3d12::RenderPassTextures::ShadowCascade2, pCascade2);
    renderPassPBRAnimated_.initResources(
        0, gfx::d3d12::RenderPassTextures::ShadowCascade0, &pCascade0->view(1u),
        dsvDescC0, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pCascade0->view(0u)), srvDescC0
    );
    renderPassPBRAnimated_.initResources(
        1, gfx::d3d12::RenderPassTextures::ShadowCascade1, &pCascade1->view(1u),
        dsvDescC1, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pCascade1->view(0u)), srvDescC1
    );
	renderPassPBRAnimated_.initResources(
		2, gfx::d3d12::RenderPassTextures::ShadowCascade2, &pCascade2->view(1u),
		dsvDescC2, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pCascade2->view(0u)), srvDescC2
	);

	// Tessellation
    renderPassTessellation_.mapTexture(gfx::d3d12::RenderPassTextures::ShadowCascade0, pCascade0);
	renderPassTessellation_.mapTexture(gfx::d3d12::RenderPassTextures::ShadowCascade1, pCascade1);
	renderPassTessellation_.mapTexture(gfx::d3d12::RenderPassTextures::ShadowCascade2, pCascade2);
    renderPassTessellation_.initResources(
        0, gfx::d3d12::RenderPassTextures::ShadowCascade0, &pCascade0->view(1u),
        dsvDescC0, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pCascade0->view(0u)), srvDescC0
    );
	renderPassTessellation_.initResources(
		1, gfx::d3d12::RenderPassTextures::ShadowCascade1, &pCascade1->view(1u),
		dsvDescC1, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pCascade1->view(0u)), srvDescC1
	);
	renderPassTessellation_.initResources(
		2, gfx::d3d12::RenderPassTextures::ShadowCascade2, &pCascade2->view(1u),
		dsvDescC2, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pCascade2->view(0u)), srvDescC2
	);

	// Shadow Map Tessellation
    renderPassShadowMapTessellation_.mapTexture(gfx::d3d12::RenderPassTextures::ShadowCascade0, pCascade0);
	renderPassShadowMapTessellation_.mapTexture(gfx::d3d12::RenderPassTextures::ShadowCascade1, pCascade1);
	renderPassShadowMapTessellation_.mapTexture(gfx::d3d12::RenderPassTextures::ShadowCascade2, pCascade2);
    renderPassShadowMapTessellation_.initResources(
        0, gfx::d3d12::RenderPassTextures::ShadowCascade0, &pCascade0->view(1u),
        dsvDescC0, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pCascade0->view(0u)), srvDescC0
    );
    renderPassShadowMapTessellation_.initResources(
        1, gfx::d3d12::RenderPassTextures::ShadowCascade1, &pCascade1->view(1u),
        dsvDescC1, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pCascade1->view(0u)), srvDescC1
    );
    renderPassShadowMapTessellation_.initResources(
        2, gfx::d3d12::RenderPassTextures::ShadowCascade2, &pCascade2->view(1u),
        dsvDescC2, reinterpret_cast<const gfx::d3d12::DescriptorGPU*>(&pCascade2->view(0u)), srvDescC2
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
    gfx::d3d12::arrangeVBs(refModel, device, cmdList, layoutIdx + 1u,
        shaderCascadeShaodwMap_.inputLayout()
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
        shaderCascadeShadowMapAnimated_.inputLayout()
    );
}

void Renderer::init(gfx::d3d12engine::Scene& scene) {
    renderPassPBR_.init(scene);
    renderPassPBRAnimated_.init(scene);
    renderPassScreenQuad_.init(scene);
    renderPassTessellation_.init(scene);
    renderPassShadowMapTessellation_.init(scene);
	renderPassCascadeShadowMap_.init(scene);
    renderPassCascadeShadowMapAnimated_.init(scene);
}

void Renderer::render(gfx::d3d12engine::Core& core, gfx::d3d12engine::Scene& scene, gfx::d3d12::RenderTargets& renderTargets) {
    auto cmdList = core.fetchCmdList();

    renderPassPBR_.update(scene);
    renderPassPBRAnimated_.update(scene);
	renderPassCascadeShadowMap_.update(scene);
    renderPassCascadeShadowMapAnimated_.update(scene);
    renderPassScreenQuad_.update(scene);
    renderPassTessellation_.update(scene);
    renderPassShadowMapTessellation_.update(scene);

    switch (renderMode_) {
    case Mode::Color:
        cmdList.get()->ClearDepthStencilView(
            rendererTexStorage_.slot(slotKeyTexture).get<gfx::d3d12::Texture>("cascade0")->view(1u).cpuHandle(),
            D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0u, 0u, nullptr
        );

        cmdList.get()->ClearDepthStencilView(
            rendererTexStorage_.slot(slotKeyTexture).get<gfx::d3d12::Texture>("cascade1")->view(1u).cpuHandle(),
            D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0u, 0u, nullptr
        );

        cmdList.get()->ClearDepthStencilView(
            rendererTexStorage_.slot(slotKeyTexture).get<gfx::d3d12::Texture>("cascade2")->view(1u).cpuHandle(),
            D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0u, 0u, nullptr
        );


        for (int i = 0; i < 3; ++i)
        {
			shaderCascadeShaodwMap_.curCascadeIdx_ = i;
            shaderCascadeShaodwMap_.bindRootParams(cmdList);
            renderPassCascadeShadowMap_.preRender(cmdList, renderTargets);
            renderPassCascadeShadowMap_.render(cmdList, renderTargets);
            renderPassCascadeShadowMap_.postRender(cmdList, renderTargets);
        }

        for (int i = 0; i < 3; ++i)
        {
            shaderCascadeShadowMapAnimated_.curCascadeIdx_ = i;
            shaderCascadeShadowMapAnimated_.bindRootParams(cmdList);
            renderPassCascadeShadowMapAnimated_.preRender(cmdList, renderTargets);
            renderPassCascadeShadowMapAnimated_.render(cmdList, renderTargets);
            renderPassCascadeShadowMapAnimated_.postRender(cmdList, renderTargets);
        }

        for (int i = 0; i < 3; ++i)
        {
			shaderShadowMapTessellation_.curCascadeIdx_ = i;
            shaderShadowMapTessellation_.bindRootParams(cmdList);
            renderPassShadowMapTessellation_.preRender(cmdList, renderTargets);
            renderPassShadowMapTessellation_.render(cmdList, renderTargets);
            renderPassShadowMapTessellation_.postRender(cmdList, renderTargets);
        }

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
			rendererTexStorage_.slot(slotKeyTexture).get<gfx::d3d12::Texture>("cascade0")->view(1u).cpuHandle(),
			D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0u, 0u, nullptr
		);

		cmdList.get()->ClearDepthStencilView(
			rendererTexStorage_.slot(slotKeyTexture).get<gfx::d3d12::Texture>("cascade1")->view(1u).cpuHandle(),
			D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0u, 0u, nullptr
		);

		cmdList.get()->ClearDepthStencilView(
			rendererTexStorage_.slot(slotKeyTexture).get<gfx::d3d12::Texture>("cascade2")->view(1u).cpuHandle(),
			D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0u, 0u, nullptr
		);

        for (int i = 0; i < 3; ++i)
        {
            shaderCascadeShaodwMap_.curCascadeIdx_ = i;
            shaderCascadeShaodwMap_.bindRootParams(cmdList);
            renderPassCascadeShadowMap_.preRender(cmdList, renderTargets);
            renderPassCascadeShadowMap_.render(cmdList, renderTargets);
            renderPassCascadeShadowMap_.postRender(cmdList, renderTargets);
        }

        for (int i = 0; i < 3; ++i)
        {
            shaderCascadeShadowMapAnimated_.curCascadeIdx_ = i;
            shaderCascadeShadowMapAnimated_.bindRootParams(cmdList);
            renderPassCascadeShadowMapAnimated_.preRender(cmdList, renderTargets);
            renderPassCascadeShadowMapAnimated_.render(cmdList, renderTargets);
            renderPassCascadeShadowMapAnimated_.postRender(cmdList, renderTargets);
        }

        for (int i = 0; i < 3; ++i) {
            shaderShadowMapTessellation_.curCascadeIdx_ = i;
            shaderShadowMapTessellation_.bindRootParams(cmdList);
            renderPassShadowMapTessellation_.preRender(cmdList, renderTargets);
            renderPassShadowMapTessellation_.render(cmdList, renderTargets);
            renderPassShadowMapTessellation_.postRender(cmdList, renderTargets);
        }

        shaderScreenQuad_.bindRootParams( cmdList );
        shaderScreenQuad_.screenQuad_.link(rendererTexStorage_.slot(slotKeyTexture).get<gfx::d3d12::Texture>("cascade0"));
        renderPassScreenQuad_.preRender( cmdList, renderTargets );
        renderPassScreenQuad_.render( cmdList, renderTargets );
        renderPassScreenQuad_.postRender( cmdList, renderTargets );
        break;

    case Mode::Cascade1Depth:
        cmdList.get()->ClearDepthStencilView(
            rendererTexStorage_.slot(slotKeyTexture).get<gfx::d3d12::Texture>("cascade0")->view(1u).cpuHandle(),
            D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0u, 0u, nullptr
        );

        cmdList.get()->ClearDepthStencilView(
            rendererTexStorage_.slot(slotKeyTexture).get<gfx::d3d12::Texture>("cascade1")->view(1u).cpuHandle(),
            D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0u, 0u, nullptr
        );

        cmdList.get()->ClearDepthStencilView(
            rendererTexStorage_.slot(slotKeyTexture).get<gfx::d3d12::Texture>("cascade2")->view(1u).cpuHandle(),
            D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0u, 0u, nullptr
        );

        for (int i = 0; i < 3; ++i)
        {
            shaderCascadeShaodwMap_.curCascadeIdx_ = i;
            shaderCascadeShaodwMap_.bindRootParams(cmdList);
            renderPassCascadeShadowMap_.preRender(cmdList, renderTargets);
            renderPassCascadeShadowMap_.render(cmdList, renderTargets);
            renderPassCascadeShadowMap_.postRender(cmdList, renderTargets);
        }

        for (int i = 0; i < 3; ++i)
        {
            shaderCascadeShadowMapAnimated_.curCascadeIdx_ = i;
            shaderCascadeShadowMapAnimated_.bindRootParams(cmdList);
            renderPassCascadeShadowMapAnimated_.preRender(cmdList, renderTargets);
            renderPassCascadeShadowMapAnimated_.render(cmdList, renderTargets);
            renderPassCascadeShadowMapAnimated_.postRender(cmdList, renderTargets);
        }

        for (int i = 0; i < 3; ++i) {
            shaderShadowMapTessellation_.curCascadeIdx_ = i;
            shaderShadowMapTessellation_.bindRootParams(cmdList);
            renderPassShadowMapTessellation_.preRender(cmdList, renderTargets);
            renderPassShadowMapTessellation_.render(cmdList, renderTargets);
            renderPassShadowMapTessellation_.postRender(cmdList, renderTargets);
        }

        shaderScreenQuad_.bindRootParams(cmdList);
        shaderScreenQuad_.screenQuad_.link(rendererTexStorage_.slot(slotKeyTexture).get<gfx::d3d12::Texture>("cascade1"));
        renderPassScreenQuad_.preRender(cmdList, renderTargets);
        renderPassScreenQuad_.render(cmdList, renderTargets);
        renderPassScreenQuad_.postRender(cmdList, renderTargets);
        break;

    case Mode::Cascade2Depth:
        cmdList.get()->ClearDepthStencilView(
            rendererTexStorage_.slot(slotKeyTexture).get<gfx::d3d12::Texture>("cascade0")->view(1u).cpuHandle(),
            D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0u, 0u, nullptr
        );

        cmdList.get()->ClearDepthStencilView(
            rendererTexStorage_.slot(slotKeyTexture).get<gfx::d3d12::Texture>("cascade1")->view(1u).cpuHandle(),
            D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0u, 0u, nullptr
        );

        cmdList.get()->ClearDepthStencilView(
            rendererTexStorage_.slot(slotKeyTexture).get<gfx::d3d12::Texture>("cascade2")->view(1u).cpuHandle(),
            D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0u, 0u, nullptr
        );

        for (int i = 0; i < 3; ++i)
        {
            shaderCascadeShaodwMap_.curCascadeIdx_ = i;
            shaderCascadeShaodwMap_.bindRootParams(cmdList);
            renderPassCascadeShadowMap_.preRender(cmdList, renderTargets);
            renderPassCascadeShadowMap_.render(cmdList, renderTargets);
            renderPassCascadeShadowMap_.postRender(cmdList, renderTargets);
        }

        for (int i = 0; i < 3; ++i)
        {
            shaderCascadeShadowMapAnimated_.curCascadeIdx_ = i;
            shaderCascadeShadowMapAnimated_.bindRootParams(cmdList);
            renderPassCascadeShadowMapAnimated_.preRender(cmdList, renderTargets);
            renderPassCascadeShadowMapAnimated_.render(cmdList, renderTargets);
            renderPassCascadeShadowMapAnimated_.postRender(cmdList, renderTargets);
        }

        for (int i = 0; i < 3; ++i) {
            shaderShadowMapTessellation_.curCascadeIdx_ = i;
            shaderShadowMapTessellation_.bindRootParams(cmdList);
            renderPassShadowMapTessellation_.preRender(cmdList, renderTargets);
            renderPassShadowMapTessellation_.render(cmdList, renderTargets);
            renderPassShadowMapTessellation_.postRender(cmdList, renderTargets);
        }

        shaderScreenQuad_.bindRootParams(cmdList);
        shaderScreenQuad_.screenQuad_.link(rendererTexStorage_.slot(slotKeyTexture).get<gfx::d3d12::Texture>("cascade2"));
        renderPassScreenQuad_.preRender(cmdList, renderTargets);
        renderPassScreenQuad_.render(cmdList, renderTargets);
        renderPassScreenQuad_.postRender(cmdList, renderTargets);
        break;

    default:
        throw GFX_EXCEPT("[Description] Unknown render mode");
    }
}