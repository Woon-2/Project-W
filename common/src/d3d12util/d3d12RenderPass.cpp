#include "d3d12util/d3d12RenderPass.hpp"

#include <ranges>
#include <algorithm>
#include <tuple>

namespace gfx {


namespace d3d12 {

void Camera::updateView() {
    repPos_ = mu::Vec3(coordMovement_.xform().row(3));
    if (focusMode_ == FocusMode::None) {
        repUp_ = mu::Vec3(coordRotation_.xform().row(1));
    }

    switch (focusMode_) {
    case FocusMode::LookAt:
        repFwd_ = mu::normalize(focus_ - repPos_);
        view_ = mu::lookAt(repPos_, focus_, repUp_);
        break;

    case FocusMode::LookTo:
        repFwd_ = mu::normalize(focus_);
        view_ = mu::lookAt(repPos_, repPos_ + repFwd_, repUp_);
        break;

    case FocusMode::None:
        repFwd_ = mu::normalize(mu::Vec3(coordRotation_.xform().row(2)));
        view_ = mu::lookAt(repPos_, repPos_ + repFwd_, repUp_);
        break;
    }
}

void RenderTargets::pushTarget(D3D12GfxCmdList& cmdList, Specifier spec, IRenderTarget* pTarget) {
    if (map_.contains(spec)) {
        throw GFX_EXCEPT( std::string("RenderTarget with the specifier")
            + sSpecifierStrings[etoi(spec)] + "already exists."
        );
    }
    map_[spec] = pTarget;
    pTarget->onPush(cmdList);
}

IRenderTarget* RenderTargets::popTarget(D3D12GfxCmdList& cmdList, Specifier spec) {
    if (!map_.contains(spec)) {
        throw GFX_EXCEPT( std::string("RenderTarget with the specifier")
            + sSpecifierStrings[etoi(spec)] + "does not exist."
        );
    }
    auto itTarget = map_.find(spec);
    auto pTarget = itTarget->second;
    map_.erase(itTarget);
    pTarget->onPop(cmdList);
    return pTarget;
}

std::string RenderTargets::sSpecifierStrings[etoi(RenderTargets::Specifier::SIZE)] = {
    "Main", "Shadow"
};

ShadowMaterial::ShadowMaterial( Texture& mapResource,
    const DescriptorCPU& dsv, const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
    const DescriptorGPU& srv, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
) : dsvDesc_(dsvDesc), srvDesc_(srvDesc), srv_(srv), dsv_(dsv),
    mapResource_(mapResource) {}

void ShadowMaterial::onPush(D3D12GfxCmdList& cmdList) {
    mapResource_.commitState(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

void ShadowMaterial::onPop(D3D12GfxCmdList& cmdList) {
    mapResource_.commitState(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void ShadowMaterial::onBind(D3D12GfxCmdList& cmdList) {
    cmdList.get()->OMSetRenderTargets(0u, nullptr, false, &dsv_.cpuHandle());
}

void ShadowMaterial::onClear(D3D12GfxCmdList& cmdList) {
    cmdList.get()->ClearDepthStencilView(
        dsv_.cpuHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0u, 0u, nullptr
    );
}

sr::Light WorldLight::toViewLight(const Camera& camera) const {
    const auto view = camera.view();

    return sr::Light{
        .color = color.getXmf(),
        .falloff = falloff,
        .posV = mu::Vec3( mu::Vec4(pos, 1.0f) * view ).getXmf(),
        .cosTheta = cosTheta,
        .dirV = mu::Vec3( mu::Vec4(dir, 0.0f) * view ).getXmf(),
        .cosPhi = cosPhi,
        .atten = atten.getXmf(),
        .intensity = intensity,
        .type = etoi(type)
    };
}

namespace rp {

PBRIllumination::PBRIllumination( D3D12Device& device,
    ShaderPBRIllumination& shader, const D3D12_VIEWPORT& vp
) : gfx::d3d12::RenderPass(id),
    viewport_(vp), protocol_( shader.makeProtocol( device,
        RenderProtocol::Desc{ makeDesc() }
    ) ), lights_(), batch_(), pCamera_(nullptr) {
    auto pcd = sr::PerConfigurationData0{
        .viewportWidth = vp.Width,
        .viewportHeight = vp.Height
    };

    shader.perConfigurationData_.stage(&pcd, sizeof(sr::PerConfigurationData0));
}

RenderProtocol::Desc PBRIllumination::makeDesc() {
    return RenderProtocol::Desc {
        .blend = D3D12_BLEND_DESC{
            .AlphaToCoverageEnable = false,
            .IndependentBlendEnable = false,
            .RenderTarget = {
                D3D12_RENDER_TARGET_BLEND_DESC{
                    .BlendEnable = false,
                    .LogicOpEnable = false,
                    .SrcBlend = D3D12_BLEND_ONE,
                    .DestBlend = D3D12_BLEND_ZERO,
                    .BlendOp = D3D12_BLEND_OP_ADD,
                    .SrcBlendAlpha = D3D12_BLEND_ONE,
                    .DestBlendAlpha = D3D12_BLEND_ZERO,
                    .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                    .LogicOp = D3D12_LOGIC_OP_NOOP,
                    .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
                }
            }
        },
        .sampleMask = UINT_MAX,
        .rasterizerState = D3D12_RASTERIZER_DESC{
            .FillMode = D3D12_FILL_MODE_SOLID,
            .CullMode = D3D12_CULL_MODE_BACK,
            .DepthClipEnable = true
        },
        .depthStencilState = D3D12_DEPTH_STENCIL_DESC{
            .DepthEnable = true,
            .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
            .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
            .StencilEnable = false
        },
        .primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .numRenderTargets = 1u,
        .rtvFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
        .dsvFormat = DXGI_FORMAT_D32_FLOAT,
        .sampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
        .nodeMask = 0u
    };
}

void PBRIllumination::setViewport(const D3D12_VIEWPORT& vp) {
    viewport_ = vp;

    auto pcd = sr::PerConfigurationData0{
        .viewportWidth = vp.Width,
        .viewportHeight = vp.Height
    };

    shader().perConfigurationData_.stage(&pcd, sizeof(sr::PerConfigurationData0));
}


void PBRIllumination::preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    cmdList.get()->SetPipelineState( protocol_.get().Get() );
    cmdList.get()->RSSetViewports(1u, &viewport_);
    auto scissorRect = D3D12_RECT{ 0, 0, static_cast<LONG>(viewport_.Width), static_cast<LONG>(viewport_.Height) };
    cmdList.get()->RSSetScissorRects(1u, &scissorRect);

    std::ranges::sort( batch_, std::less<>{}, [this](const auto& tuple) {
        return std::tuple(
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
    } );

    auto pids = std::vector<sr::PerInstanceData0>();
    pids.reserve( shader().maxInstanceCnt() );

    for (auto& [pSubmesh, vbLayoutIdx, xform] : batch_) {
        xform = pSubmesh->parent()->parent()->coord().xform();
        pids.emplace_back(
            /* .wvp = */ mu::transpose( xform * pCamera_->view() * pCamera_->proj() ).getXmf(),
            /* .world = */ mu::transpose( xform ).getXmf(),
            /* .wv = */ mu::transpose( xform * pCamera_->view() ).getXmf(),
            /* .wvNormal = */ dx::convertMat<dx::XMFLOAT3X3>(
                mu::inverse(xform * pCamera_->view()).get()
            )
        );

        if (pids.size() == shader().maxInstanceCnt()) [[unlikely]] {
            break;
        }
    }

    auto lightBuffer = std::vector<sr::Light>();
    lightBuffer.reserve( lights_.size() );

    for (const auto& light : lights_) {
        lightBuffer.emplace_back( light->toViewLight(*pCamera_) );

        if (lightBuffer.size() == shader().maxLightCnt()) [[unlikely]] {
            break;
        }
    }

    auto pfd = sr::PerFrameData0{
        .globalAmbient = dx::XMFLOAT3(0.1f, 0.1f, 0.1f),
        .lightCnt = static_cast<std::uint32_t>( lightBuffer.size() )
    };

    shader().perInstanceData_.stage(pids.data(), pids.size() * sizeof(sr::PerInstanceData0));
    shader().lightBuffer_.stage(lightBuffer.data(), lightBuffer.size() * sizeof(sr::Light));
    shader().perFrameData_.stage(&pfd, sizeof(sr::PerFrameData0));
}

void PBRIllumination::render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    renderTargets.bind(cmdList, RenderTargets::Specifier::Main);

    auto first = batch_.begin();
    auto accDrawcallCnt = 0u;
    while (first != batch_.end()) {
        auto proj = [this](const auto& tuple) {
            return std::tuple(
                &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
                std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
            );
        };

        auto last = std::ranges::upper_bound(first, batch_.end(), proj(*first), std::less<>{}, proj);

        auto pSubmesh = std::get<gfx::d3d12::Submesh*>(*first);

        auto material = sr::PBRMaterial::convert( pSubmesh->material() );

        auto pdd = sr::PerDrawcallData0{
            .material = material,
            .instanceBase = static_cast<std::uint32_t>(first - batch_.begin()),
            .samplerIdx = 0u
        };
        shader().perDrawcallData_.stage( &pdd, sizeof(sr::PerDrawcallData0),
            0u, accDrawcallCnt * shader().cbDrawcallDataSize()
        );

        shader().bindPerDrawcallData(accDrawcallCnt++, cmdList);

        shader().draw( cmdList, *pSubmesh, static_cast<std::size_t>(last - first),
            std::get<VBLayoutIdx>(*first)
        );

        if (accDrawcallCnt == shader().maxDrawcallCnt()) {
            break;
        }

        first = last;
    }
}

void PBRIllumination::postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {

}

void PBRIllumination::trackModel(Model* pModel) {
    if (!pModel->markedRenderPasses().empty()) {
        auto it = std::ranges::find(pModel->markedRenderPasses(), renderPassID());
        if (it == pModel->markedRenderPasses().end()) {
            return;
        }
    }

    auto& nodes = pModel->nodes();

    for (auto& node : nodes) {
        auto xform = node.coord().xform();
        for (auto& mesh : node.meshes()) {
            auto vbLayoutIdx = protocol_.compatibleLayout(*mesh.refMesh());
            if (!vbLayoutIdx) {
                throw std::runtime_error("Incompatible mesh");
            }

            for (auto& submesh : mesh.submeshes()) {
                batch_.emplace_back(&submesh, vbLayoutIdx.value(), xform);
            }
        }
    }
}

PBRIlluminationTerrain::PBRIlluminationTerrain( D3D12Device& device,
    ShaderPBRIlluminationTerrain& shader, const D3D12_VIEWPORT& vp
) : gfx::d3d12::RenderPass(id),
    viewport_(vp), protocol_( shader.makeProtocol( device,
        RenderProtocol::Desc{ makeDesc() }
    ) ), lights_(), batch_(), pCamera_(nullptr) {
    auto pcd = sr::PerConfigurationData0{
        .viewportWidth = vp.Width,
        .viewportHeight = vp.Height
    };

    shader.perConfigurationData_.stage(&pcd, sizeof(sr::PerConfigurationData0));
}

RenderProtocol::Desc PBRIlluminationTerrain::makeDesc() {
    return RenderProtocol::Desc {
        .blend = D3D12_BLEND_DESC{
            .AlphaToCoverageEnable = false,
            .IndependentBlendEnable = false,
            .RenderTarget = {
                D3D12_RENDER_TARGET_BLEND_DESC{
                    .BlendEnable = false,
                    .SrcBlend = D3D12_BLEND_ONE,
                    .DestBlend = D3D12_BLEND_ZERO,
                    .BlendOp = D3D12_BLEND_OP_ADD,
                    .SrcBlendAlpha = D3D12_BLEND_ONE,
                    .DestBlendAlpha = D3D12_BLEND_ZERO,
                    .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                    .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
                }
            }
        },
        .sampleMask = UINT_MAX,
        .rasterizerState = D3D12_RASTERIZER_DESC{
            .FillMode = D3D12_FILL_MODE_SOLID,
            .CullMode = D3D12_CULL_MODE_BACK,
            .DepthClipEnable = true,
        },
        .depthStencilState = D3D12_DEPTH_STENCIL_DESC{
            .DepthEnable = true,
            .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
            .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
            .StencilEnable = false
        },
        .primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH,
        .numRenderTargets = 1u,
        .rtvFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
        .dsvFormat = DXGI_FORMAT_D32_FLOAT,
        .sampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
        .nodeMask = 0u
    };
}

void PBRIlluminationTerrain::setViewport(const D3D12_VIEWPORT& vp) {
    viewport_ = vp;

    auto pcd = sr::PerConfigurationData0{
        .viewportWidth = vp.Width,
        .viewportHeight = vp.Height
    };

    shader().perConfigurationData_.stage(&pcd, sizeof(sr::PerConfigurationData0));
}


void PBRIlluminationTerrain::preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    cmdList.get()->SetPipelineState( protocol_.get().Get() );
    cmdList.get()->RSSetViewports(1u, &viewport_);
    auto scissorRect = D3D12_RECT{ 0, 0, static_cast<LONG>(viewport_.Width), static_cast<LONG>(viewport_.Height) };
    cmdList.get()->RSSetScissorRects(1u, &scissorRect);

    std::ranges::sort( batch_, std::less<>{}, [this](const auto& tuple) {
        return std::tuple(
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
    } );

    auto pids = std::vector<sr::PerInstanceData1>();
    pids.reserve( shader().maxInstanceCnt() );

    for (auto& [pSubmesh, vbLayoutIdx, xform] : batch_) {
        xform = pSubmesh->parent()->parent()->coord().xform();
        pids.emplace_back(
            /* .wv = */ mu::transpose( xform * pCamera_->view() ).getXmf(),
            /* .proj = */ mu::transpose( pCamera_->proj() ).getXmf(),
            /* .wvNormal = */ dx::convertMat<dx::XMFLOAT3X3>(
                mu::inverse(xform * pCamera_->view()).get()
            )
        );

        if (pids.size() == shader().maxInstanceCnt()) [[unlikely]] {
            break;
        }
    }

    auto lightBuffer = std::vector<sr::Light>();
    lightBuffer.reserve( lights_.size() );

    for (const auto& light : lights_) {
        lightBuffer.emplace_back( light->toViewLight(*pCamera_) );

        if (lightBuffer.size() == shader().maxLightCnt()) [[unlikely]] {
            break;
        }
    }

    auto pfd = sr::PerFrameData0{
        .globalAmbient = dx::XMFLOAT3(0.1f, 0.1f, 0.1f),
        .lightCnt = static_cast<std::uint32_t>( lightBuffer.size() )
    };

    shader().perInstanceData_.stage(pids.data(), pids.size() * sizeof(sr::PerInstanceData0));
    shader().lightBuffer_.stage(lightBuffer.data(), lightBuffer.size() * sizeof(sr::Light));
    shader().perFrameData_.stage(&pfd, sizeof(sr::PerFrameData0));
}

void PBRIlluminationTerrain::render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    renderTargets.bind(cmdList, RenderTargets::Specifier::Main);

    auto first = batch_.begin();
    auto accDrawcallCnt = 0u;

    while (first != batch_.end()) {
        auto proj = [this](const auto& tuple) {
            return std::tuple(
                &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
                std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
            );
        };

        auto last = std::ranges::upper_bound(first, batch_.end(), proj(*first), std::less<>{}, proj);

        auto pSubmesh = std::get<gfx::d3d12::Submesh*>(*first);

        auto material = sr::PBRMaterial::convert( pSubmesh->material() );

        auto pdd = sr::PerDrawcallData0{
            .material = material,
            .instanceBase = static_cast<std::uint32_t>(first - batch_.begin()),
            .samplerIdx = 0u
        };
        shader().perDrawcallData_.stage( &pdd, sizeof(sr::PerDrawcallData0),
            0u, accDrawcallCnt * shader().cbDrawcallDataSize()
        );

        shader().bindPerDrawcallData(accDrawcallCnt++, cmdList);

        shader().draw( cmdList, *pSubmesh, static_cast<std::size_t>(last - first),
            std::get<VBLayoutIdx>(*first)
        );

        if (accDrawcallCnt == shader().maxDrawcallCnt()) {
            break;
        }

        first = last;
    }
}

void PBRIlluminationTerrain::postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {

}

void PBRIlluminationTerrain::trackModel(Model* pModel) {
    if (!pModel->markedRenderPasses().empty()) {
        auto it = std::ranges::find(pModel->markedRenderPasses(), renderPassID());
        if (it == pModel->markedRenderPasses().end()) {
            return;
        }
    }

    auto& nodes = pModel->nodes();

    for (auto& node : nodes) {
        auto xform = node.coord().xform();
        for (auto& mesh : node.meshes()) {
            auto vbLayoutIdx = protocol_.compatibleLayout(*mesh.refMesh());
            if (!vbLayoutIdx) {
                throw std::runtime_error("Incompatible mesh");
            }

            for (auto& submesh : mesh.submeshes()) {
                batch_.emplace_back(&submesh, vbLayoutIdx.value(), xform);
            }
        }
    }
}


ShadowMap::ShadowMap( D3D12Device& device, ShaderShadowMap& shader,
    DescriptorRange<DescriptorHeapCPU>& dsvRange, const D3D12_VIEWPORT& vp
) : gfx::d3d12::RenderPass(id),
    shadowMapSrvDesc_{
        .Format = convertToColorFormat( shader.shadowMap_.desc().Format ),
        .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Texture2D = D3D12_TEX2D_SRV{ .MipLevels = 1u }
    },
    shadowMapDsvDesc_{
        .Format = shader.shadowMap_.desc().Format,
        .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
        .Flags = D3D12_DSV_FLAG_NONE
    },
    idxShadowMapDsv_( shader.shadowMap_.makeDsv( shadowMapDsvDesc_, device, dsvRange.alloc() ) ),
    shadowMaterial_( shader.shadowMap_, shader.shadowMap_.view( idxShadowMapDsv_ ),
        shadowMapDsvDesc_, shader.shadowMap_.view( shader.shadowMap_.idxSrv ),
        shadowMapSrvDesc_
    ),
    viewport_(vp), protocol_( shader.makeProtocol( device,
        RenderProtocol::Desc{ makeDesc() }
    ) ), pLight_(nullptr), batch_(), pCamera_(nullptr) {}

RenderProtocol::Desc ShadowMap::makeDesc() {
    return RenderProtocol::Desc {
        .blend = D3D12_BLEND_DESC{
            .AlphaToCoverageEnable = false,
            .IndependentBlendEnable = false,
            .RenderTarget = {
                D3D12_RENDER_TARGET_BLEND_DESC{
                    .BlendEnable = false,
                    .SrcBlend = D3D12_BLEND_ONE,
                    .DestBlend = D3D12_BLEND_ZERO,
                    .BlendOp = D3D12_BLEND_OP_ADD,
                    .SrcBlendAlpha = D3D12_BLEND_ONE,
                    .DestBlendAlpha = D3D12_BLEND_ZERO,
                    .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                    .RenderTargetWriteMask = 0u
                }
            }
        },
        .sampleMask = UINT_MAX,
        .rasterizerState = D3D12_RASTERIZER_DESC{
            .FillMode = D3D12_FILL_MODE_SOLID,
            .CullMode = D3D12_CULL_MODE_BACK,
            .DepthBias = 100000,
            .DepthBiasClamp = 0.0f,
            .SlopeScaledDepthBias = 1.0f,
            .DepthClipEnable = true
        },
        .depthStencilState = D3D12_DEPTH_STENCIL_DESC{
            .DepthEnable = true,
            .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
            .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
            .StencilEnable = false
        },
        .primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .numRenderTargets = 1u,
        .rtvFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
        .dsvFormat = DXGI_FORMAT_D32_FLOAT,
        .sampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
        .nodeMask = 0u
    };
}

void ShadowMap::setViewport(const D3D12_VIEWPORT& vp) {
    viewport_ = vp;
}

void ShadowMap::preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    cmdList.get()->SetPipelineState( protocol_.get().Get() );
    cmdList.get()->RSSetViewports(1u, &viewport_);
    auto scissorRect = D3D12_RECT{ 0, 0, static_cast<LONG>(viewport_.Width), static_cast<LONG>(viewport_.Height) };
    cmdList.get()->RSSetScissorRects(1u, &scissorRect);
    renderTargets.pushTarget(cmdList, RenderTargets::Specifier::Shadow, &shadowMaterial_);

    std::ranges::sort( batch_, std::less<>{}, [this](const auto& tuple) {
        return std::tuple(
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
    } );

    auto pids = std::vector<sr::PerInstanceData2>();
    pids.reserve( shader().maxInstanceCnt() );

    for (auto& [pSubmesh, vbLayoutIdx, xform] : batch_) {
        xform = pSubmesh->parent()->parent()->coord().xform();
        pids.emplace_back(
            /* .world = */ mu::transpose( xform ).getXmf()
        );

        if (pids.size() == shader().maxInstanceCnt()) [[unlikely]] {
            break;
        }
    }

    const auto lightPos = mu::Vec3( pCamera_->repPos() - pLight_->dir * 800.f );
    const auto lightDir = pLight_->dir;

    const auto lightView = mu::lookAt( lightPos, lightPos + lightDir, mu::Vec3( 0.f, 1.f, 0.f ) );
    const auto lightProj = mu::ortho( -10.f, 10.f, -10.f, 10.f, 600.f, 1200.f );

    const auto pfd = sr::PerFrameData1{
        .lightVP = mu::transpose( lightView * lightProj ).getXmf()
    };

    shader().perInstanceData_.stage(pids.data(), pids.size() * sizeof(sr::PerInstanceData2));
    shader().perFrameData_.stage(&pfd, sizeof(sr::PerFrameData1));
}

void ShadowMap::render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    renderTargets.bind(cmdList, RenderTargets::Specifier::Shadow);
    renderTargets.clear(cmdList, RenderTargets::Specifier::Shadow);

    auto first = batch_.begin();
    auto accDrawcallCnt = 0u;

    while (first != batch_.end()) {
        auto proj = [this](const auto& tuple) {
            return std::tuple(
                &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
                std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
            );
        };

        auto last = std::ranges::upper_bound(first, batch_.end(), proj(*first), std::less<>{}, proj);

        auto pSubmesh = std::get<gfx::d3d12::Submesh*>(*first);

        auto pdd = sr::PerDrawcallData2{
            .instanceBase = static_cast<std::uint32_t>(first - batch_.begin()),
        };
        shader().perDrawcallData_.stage( &pdd, sizeof(sr::PerDrawcallData2),
            0u, accDrawcallCnt * shader().cbDrawcallDataSize()
        );

        shader().bindPerDrawcallData(accDrawcallCnt++, cmdList);

        shader().draw( cmdList, *pSubmesh, static_cast<std::size_t>(last - first),
            std::get<VBLayoutIdx>(*first)
        );

        if (accDrawcallCnt == shader().maxDrawcallCnt()) {
            break;
        }

        first = last;
    }
}

void ShadowMap::postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {

}

void ShadowMap::trackModel(Model* pModel) {
    if (!pModel->markedRenderPasses().empty()) {
        auto it = std::ranges::find(pModel->markedRenderPasses(), renderPassID());
        if (it == pModel->markedRenderPasses().end()) {
            return;
        }
    }

    auto& nodes = pModel->nodes();

    for (auto& node : nodes) {
        auto xform = node.coord().xform();
        for (auto& mesh : node.meshes()) {
            auto vbLayoutIdx = protocol_.compatibleLayout(*mesh.refMesh());
            if (!vbLayoutIdx) {
                throw std::runtime_error("Incompatible mesh");
            }

            for (auto& submesh : mesh.submeshes()) {
                batch_.emplace_back(&submesh, vbLayoutIdx.value(), xform);
            }
        }
    }
}

void ShadowMap::setLight(const WorldLight* pLight) {
    pLight_ = pLight;
}

ScreenQuad::ScreenQuad( D3D12Device& device, ShaderScreenQuad& shader,
    const D3D12_VIEWPORT& vp
) : gfx::d3d12::RenderPass(id),
    viewport_(vp), protocol_( shader.makeProtocol( device,
        RenderProtocol::Desc{ makeDesc() }
    ) ) {}

RenderProtocol::Desc ScreenQuad::makeDesc() {
    return RenderProtocol::Desc {
        .blend = D3D12_BLEND_DESC{
            .AlphaToCoverageEnable = false,
            .IndependentBlendEnable = false,
            .RenderTarget = {
                D3D12_RENDER_TARGET_BLEND_DESC{
                    .BlendEnable = false,
                    .SrcBlend = D3D12_BLEND_ONE,
                    .DestBlend = D3D12_BLEND_ZERO,
                    .BlendOp = D3D12_BLEND_OP_ADD,
                    .SrcBlendAlpha = D3D12_BLEND_ONE,
                    .DestBlendAlpha = D3D12_BLEND_ZERO,
                    .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                    .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
                }
            }
        },
        .sampleMask = UINT_MAX,
        .rasterizerState = D3D12_RASTERIZER_DESC{
            .FillMode = D3D12_FILL_MODE_SOLID,
            .CullMode = D3D12_CULL_MODE_NONE,
            .DepthClipEnable = true
        },
        .depthStencilState = D3D12_DEPTH_STENCIL_DESC{
            .DepthEnable = false,
            .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
            .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
            .StencilEnable = false
        },
        .primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .numRenderTargets = 1u,
        .rtvFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
        .dsvFormat = DXGI_FORMAT_D32_FLOAT,
        .sampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
        .nodeMask = 0u
    };
}

void ScreenQuad::setViewport(const D3D12_VIEWPORT& vp) {
    viewport_ = vp;
}

void ScreenQuad::preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    cmdList.get()->SetPipelineState( protocol_.get().Get() );
    cmdList.get()->RSSetViewports(1u, &viewport_);
    auto scissorRect = D3D12_RECT{ 0, 0, static_cast<LONG>(viewport_.Width), static_cast<LONG>(viewport_.Height) };
    cmdList.get()->RSSetScissorRects(1u, &scissorRect);
}

void ScreenQuad::render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    renderTargets.bind(cmdList, RenderTargets::Specifier::Main);
    renderTargets.clear(cmdList, RenderTargets::Specifier::Main);

    const auto pdd = sr::PerDrawcallData3{
        .frameMapRef = shader().screenQuad_.mapRef().toxm(),
        .samplerIdx = 0u
    };

    shader().perDrawcallData_.stage(&pdd, sizeof(sr::PerDrawcallData3));

    shader().draw( cmdList );
}

void ScreenQuad::postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {}

}   // namespace gfx::d3d12::rp

}   // namespace gfx::d3d12

}   // namespace gfx
