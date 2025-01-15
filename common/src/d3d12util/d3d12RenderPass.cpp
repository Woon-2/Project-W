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
        lightBuffer.emplace_back( *light );

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
        lightBuffer.emplace_back( *light );

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


ShadowMap::ShadowMap( D3D12Device& device,
    ShaderShadowMap& shader, const D3D12_VIEWPORT& vp
) : gfx::d3d12::RenderPass(id),
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
            .DepthClipEnable = true,
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

    shader().perInstanceData_.stage(pids.data(), pids.size() * sizeof(sr::PerInstanceData0));
}

void ShadowMap::render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    // cmdList.get()->OMSetRenderTargets(0u, nullptr, true, &dsv.cpuHandle());

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

void ShadowMap::setLight(const sr::Light* pLight) {
    pLight_ = pLight;

    const auto lightPos = mu::Vec3( DirectX::XMLoadFloat3(&pLight_->posV) );
    const auto lightDir = mu::Vec3( DirectX::XMLoadFloat3(&pLight_->dirV) );

    const auto lightView = mu::lookAt( lightPos, lightPos + lightDir, mu::Vec3( 0.f, 1.f, 0.f ) );
    const auto lightProj = mu::ortho( -10.f, 10.f, -10.f, 10.f, 0.1f, 100.f );

    const auto pfd = sr::PerFrameData1{
        .lightVP = mu::transpose( lightView * lightProj ).getXmf()
    };

    shader().perFrameData_.stage(&pfd, sizeof(sr::PerFrameData1));
}

}   // namespace gfx::d3d12::rp

}   // namespace gfx::d3d12

} // namespace gfx
