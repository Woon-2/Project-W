#include "d3d12util/d3d12RenderPass.hpp"

#include <ranges>
#include <algorithm>
#include <tuple>

namespace gfx {


namespace d3d12 {

void Camera::updateView() {
    auto xform = coordSys_.xform();
    repPos_ = xform.row(3);
    repUp_ = xform.row(1);

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
        repFwd_ = xform.row(2);
        view_ = mu::lookAt(repPos_, repPos_ + repFwd_, repUp_);
        break;
    }
}

namespace rp {

RenderProtocol::Desc PBRIllumination::makeDesc() {
    return RenderProtocol::Desc {
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
        },
        .primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .numRenderTargets = 1u,
        .rtvFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
        .dsvFormat = DXGI_FORMAT_D32_FLOAT,
        .sampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
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


void PBRIllumination::preRender(D3D12GfxCmdList& cmdList) {
    cmdList.get()->SetPipelineState( protocol_.get().Get() );
    cmdList.get()->RSSetViewports(1u, &viewport_);
    auto scissorRect = D3D12_RECT{ 0, 0, static_cast<LONG>(viewport_.Width), static_cast<LONG>(viewport_.Height) };
    cmdList.get()->RSSetScissorRects(1u, &scissorRect);

    std::ranges::sort(batch_, std::less<>{}, [this](const auto& pair) {
        return std::tuple(&pair.first->material(renderPassID()), &pair.first->refMesh());
    });

    auto pids = std::vector<sr::PerInstanceData0>();
    pids.reserve( shader().maxInstanceCnt() );

    for (const auto& [pMesh, xform] : batch_) {
        pids.emplace_back(
            /* .wvp = */ mu::transpose( xform * pCamera_->view() * pCamera_->proj() ).getXmf(),
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

void PBRIllumination::render(D3D12GfxCmdList& cmdList) {
    auto first = batch_.begin();
    auto accDrawcallCnt = 0u;

    while (first != batch_.end()) {
        auto proj = [this](const auto& pair) {
            return std::tuple(&pair.first->material(renderPassID()), &pair.first->refMesh());
        };

        auto last = std::ranges::upper_bound(first, batch_.end(), proj(*first), std::less<>{}, proj);

        auto pMesh = first->first;

        auto material = sr::PBRMaterial::convert( pMesh->material(renderPassID()) );

        auto pdd = sr::PerDrawcallData0{
            .material = material,
            .samplerIdx = 0u
        };
        shader().perDrawcallData_.stage( &pdd, sizeof(sr::PerDrawcallData0),
            0u, accDrawcallCnt++ * sizeof(sr::PerDrawcallData0)
        );

        shader().draw( cmdList, *pMesh, static_cast<std::size_t>(last - first) );

        if (accDrawcallCnt == shader().maxDrawcallCnt()) {
            break;
        }

        first = last;
    }
}

void PBRIllumination::postRender(D3D12GfxCmdList& cmdList) {

}

void PBRIllumination::trackModel(Model* pModel) {
    auto& nodes = pModel->nodes();

    for (auto& node : nodes) {
        auto xform = node.coord().xform();
        for (auto& mesh : node.meshes()) {
            if (!protocol_.compatibleWith(mesh.refMesh())) {
                throw std::runtime_error("Incompatible mesh");
            }

            batch_.emplace_back(&mesh, xform);
        }
    }
}

RenderProtocol::Desc PBRIlluminationMacro::makeDesc() {
    return RenderProtocol::Desc {
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
        },
        .primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .numRenderTargets = 1u,
        .rtvFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
        .dsvFormat = DXGI_FORMAT_D32_FLOAT,
        .sampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
    };
}

void PBRIlluminationMacro::setViewport(const D3D12_VIEWPORT& vp) {
    viewport_ = vp;

    auto pcd = sr::PerConfigurationData0{
        .viewportWidth = vp.Width,
        .viewportHeight = vp.Height
    };

    shader().perConfigurationData_.stage(&pcd, sizeof(sr::PerConfigurationData0));
}


void PBRIlluminationMacro::preRender(D3D12GfxCmdList& cmdList) {
    cmdList.get()->SetPipelineState( protocol_.get().Get() );
    cmdList.get()->RSSetViewports(1u, &viewport_);
    auto scissorRect = D3D12_RECT{ 0, 0, static_cast<LONG>(viewport_.Width), static_cast<LONG>(viewport_.Height) };
    cmdList.get()->RSSetScissorRects(1u, &scissorRect);

    std::ranges::sort(batch_, std::less<>{}, [this](const auto& pair) {
        return std::tuple(&pair.first->material(renderPassID()), &pair.first->refMesh());
    });

    auto pids = std::vector<sr::PerInstanceData0>();
    pids.reserve( shader().maxInstanceCnt() );

    for (const auto& [pMesh, xform] : batch_) {
        pids.emplace_back(
            /* .wvp = */ mu::transpose( xform * pCamera_->view() * pCamera_->proj() ).getXmf(),
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

void PBRIlluminationMacro::render(D3D12GfxCmdList& cmdList) {
    auto first = batch_.begin();
    auto accDrawcallCnt = 0u;

    while (first != batch_.end()) {
        auto proj = [this](const auto& pair) {
            return std::tuple(&pair.first->material(renderPassID()), &pair.first->refMesh());
        };

        auto last = std::ranges::upper_bound(first, batch_.end(), proj(*first), std::less<>{}, proj);

        auto pMesh = first->first;

        auto material = sr::PBRMaterial::convert( pMesh->material(renderPassID()) );

        auto pdd = sr::PerDrawcallData0{
            .material = material,
            .samplerIdx = 0u
        };
        shader().perDrawcallData_.stage( &pdd, sizeof(sr::PerDrawcallData0),
            0u, accDrawcallCnt++ * sizeof(sr::PerDrawcallData0)
        );

        shader().draw( cmdList, *pMesh, static_cast<std::size_t>(last - first) );

        if (accDrawcallCnt == shader().maxDrawcallCnt()) {
            break;
        }

        first = last;
    }
}

void PBRIlluminationMacro::postRender(D3D12GfxCmdList& cmdList) {

}

void PBRIlluminationMacro::trackModel(Model* pModel) {
    if (!pModel->willDrawOnRenderPass(renderPassID())) {
        return;
    }

    auto& nodes = pModel->nodes();

    for (auto& node : nodes) {
        auto xform = node.coord().xform();
        for (auto& mesh : node.meshes()) {
            if (!protocol_.compatibleWith(mesh.refMesh())) {
                throw std::runtime_error("Incompatible mesh");
            }

            batch_.emplace_back(&mesh, xform);
        }
    }
}

}   // namespace gfx::d3d12::rp

}   // namespace gfx::d3d12

}   // namespace gfx