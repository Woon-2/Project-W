#include "sampleRenderer.hpp"
#include "d3d12Scene.hpp"

#include "shaderPath.hpp"
#include "phongShader.hpp"

namespace gfx {

void SampleRenderer::init(ICore& core) {
    if (auto tmp = dynamic_cast<d3d12::Core*>(&core); !tmp) {
        throw;  /*CoreTypeMismatch("The core type is mismatched.");*/
    }
    
    drawer_.init(*this, static_cast<d3d12::Core&>(core));
}

// TODO: write the exception classes
void SampleRenderer::render(const IScene& scene, IRenderContext& renderContext, IRenderTarget& target) const {
    if (!renderContext.castableTo(RenderContextType::D3D12)) {
        throw;  /*RenderContextTypeMismatch("The render context type is mismatched.");*/
    }
    auto pCmdList = std::any_cast<wrl::ComPtr<ID3D12GraphicsCommandList>>(
        renderContext.cast(RenderContextType::D3D12)
    );

    if (!target.castableTo(RenderTargetType::D3D12)) {
        throw;  /*RenderTargetTypeMismatch("The render target type is mismatched.");*/
    }
    auto pTarget = std::any_cast<D3D12_CPU_DESCRIPTOR_HANDLE>(
        target.cast(RenderTargetType::D3D12)
    );

    if (!target.castableTo(RenderTargetType::D3D12_DEPTH)) {
        throw;  /*RenderTargetTypeMismatch("The render target type is mismatched.");*/
    }
    auto pDepthTarget = std::any_cast<D3D12_CPU_DESCRIPTOR_HANDLE>(
        target.cast(RenderTargetType::D3D12_DEPTH)
    );

    auto& shader = static_cast<d3d12::D3D12RenderContext&>(
        renderContext
    ).shader( d3d12::PhongShaderNT::shaderName() );

    shader.bind(pCmdList.Get(), 0);

    // æ¿¿Ã∂˚ «¡∑Œ≈‰ƒ› ∫Ò±≥
    assert(static_cast<const d3d12::CameraScene&>(scene).protocol() == protocol_);

    switch (protocol_)
    {
    case gfx::rp::Protocol::PhongInstancingNT:
        drawer_.phongInstancingNT(scene, shader, pCmdList.Get(), pTarget, pDepthTarget);
        break;

    case gfx::rp::Protocol::SomeProtocol:
        break;
    default:

        break;
    }
}

void SampleRenderer::cleanup() {}

void SampleRenderer::D3D12Drawer::init(SampleRenderer& renderer, d3d12::Core& core) {
    if (core.containsShader(d3d12::PhongShaderNT::shaderName())) {
        return;
    }

    core.addShader(d3d12::PhongShaderNT::shaderName(), d3d12::PhongShaderNT(core));
}

void SampleRenderer::D3D12Drawer::phongInstancingNT( const IScene& scene, d3d12::Shader& shader, ID3D12GraphicsCommandList* pCmdList,
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle
) const {
    // æ¿¿Ã∂˚ «¡∑Œ≈‰ƒ› ∫Ò±≥
    assert(static_cast<const d3d12::CameraScene&>(scene).protocol() == protocol_);
    assert(dynamic_cast<d3d12::PhongShaderNT*>(&shader) != nullptr);

    using Light = rp::PhongInstancingNT::LightType;
    using Material = rp::PhongInstancingNT::MaterialType;
    using PID = rp::PhongInstancingNT::PIDType;
    using PFD = rp::PhongInstancingNT::PFDType;
    using PDD = rp::PhongInstancingNT::PDDType;

    auto pShader = static_cast<d3d12::PhongShaderNT*>(&shader);

    pCmdList->OMSetRenderTargets(1u, &rtvHandle, true, &dsvHandle);
    pCmdList->SetGraphicsRootShaderResourceView(0, pShader->pid().gpuAddress());
    pCmdList->SetGraphicsRootShaderResourceView(1, pShader->materials().gpuAddress());
    pCmdList->SetGraphicsRootShaderResourceView(2, pShader->lights().gpuAddress());
    pCmdList->SetGraphicsRootConstantBufferView(3, pShader->pdd().gpuAddress());
    pCmdList->SetGraphicsRootConstantBufferView(4, pShader->pfd().gpuAddress());

    auto lightCnt = 0ull;
    auto materialCnt = 0ull;
    const d3d12::Mesh* pMesh = nullptr;
    auto instanceCnt = 0ull;
    auto pfd = PFD{};

    for (auto di : scene.iteration()) {
        auto type = di.get<rp::DIType>(rp::PhongInstancingNT::typeIdx);

        switch (type) {
        case rp::DIType::Light: {
            auto lights = di.get<std::span<Light>>(rp::PhongInstancingNT::lightIdx);
            auto cnt = std::min(d3d12::PhongShaderNT::defMaxLights - lightCnt, lights.size());
            pShader->lights().uploadRegion( lights.data(), cnt * sizeof(Light), lightCnt * sizeof(Light) );
            lightCnt += cnt;
            break;
        }

        case rp::DIType::Material: {
            auto materials = di.get<std::span<Material>>(rp::PhongInstancingNT::materialIdx);
            auto cnt = std::min(d3d12::PhongShaderNT::defMaxMaterials - materialCnt, materials.size());
            pShader->materials().uploadRegion(materials.data(), cnt * sizeof(Material), materialCnt * sizeof(Material));
            materialCnt += cnt;
            break;
        }

        case rp::DIType::Mesh: {
            pMesh = di.get<const d3d12::Mesh*>(rp::PhongInstancingNT::meshIdx);
            pMesh->bind(pCmdList);
            break;
        }

        case rp::DIType::PID: {
            auto pids = di.get<std::span<PID>>(rp::PhongInstancingNT::PIDIdx);
            auto cnt = std::min(d3d12::PhongShaderNT::defMaxInstances - instanceCnt, pids.size());
            pShader->pid().uploadRegion(pids.data(), cnt * sizeof(PID), instanceCnt * sizeof(PID));
            instanceCnt += cnt;
            break;
        }

        case rp::DIType::PFD: {
            pfd = di.get<PFD>(rp::PhongInstancingNT::PFDIdx);
            break;
        }

        case rp::DIType::PDD: {
            auto pdd = di.get<PDD>(rp::PhongInstancingNT::PDDIdx);
            pfd.lightCnt = static_cast<std::uint32_t>( lightCnt );

            pShader->pdd().upload(&pdd, sizeof(PDD));
            pShader->pfd().upload(&pfd, sizeof(PFD));

            pMesh->draw(pCmdList, instanceCnt);
            break;
        }

        default:
            throw std::runtime_error("Invalid DrawInfo type.");
        };
    }
}

} // namespace gfx