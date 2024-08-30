#include "sampleRenderer.hpp"
#include "d3d12Scene.hpp"

#include "shaderPath.hpp"
#include "solidShader.hpp"

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

    const auto& shader = static_cast<d3d12::D3D12RenderContext&>(
        renderContext
    ).shader( d3d12::SolidShader::shaderName() );

    shader.bind(pCmdList.Get(), 0);

    // æ¿¿Ã∂˚ «¡∑Œ≈‰ƒ› ∫Ò±≥
    assert(static_cast<const d3d12::CameraScene&>(scene).protocol() == protocol_);

    switch (protocol_)
    {
    case gfx::rp::Protocol::PhongInstancingNT:
        drawer_.phongInstancingNT(scene, pCmdList.Get(), pTarget, pDepthTarget);
        break;

    case gfx::rp::Protocol::SomeProtocol:
        break;
    default:

        break;
    }
}

void SampleRenderer::cleanup() {}

void SampleRenderer::D3D12Drawer::init(SampleRenderer& renderer, d3d12::Core& core) {
    if (core.containsShader(d3d12::SolidShader::shaderName())) {
        return;
    }

    core.addShader(d3d12::SolidShader::shaderName(), d3d12::SolidShader(core));

    resPerFrameData_ = d3d12::createUpBuf(core, static_cast<UINT>( sizeof(d3d12::sr::BasicPFD) ));
    resPerDrawcallData_ = d3d12::createUpBuf(core, static_cast<UINT>( sizeof(d3d12::sr::BasicPDD) ));
    resPerInstanceData_ = d3d12::createUpBuf(core, static_cast<UINT>( sizeof(d3d12::sr::BasicPID) * maxInstances ));
    resMaterials_ = d3d12::createUpBuf(core, static_cast<UINT>( sizeof(d3d12::sr::PhongMaterial) * maxMaterials ));
    resLights_ = d3d12::createUpBuf(core, static_cast<UINT>( sizeof(d3d12::sr::PhongLight) * maxLights ));

    resPerFrameData_->Map(0, nullptr, reinterpret_cast<void**>(&pPFD_));
    resPerDrawcallData_->Map(0, nullptr, reinterpret_cast<void**>(&pPDD_));
    resPerInstanceData_->Map(0, nullptr, reinterpret_cast<void**>(&pPID_));
    resMaterials_->Map(0, nullptr, reinterpret_cast<void**>(&pMats_));
    resLights_->Map(0, nullptr, reinterpret_cast<void**>(&pLights_));
}

void SampleRenderer::D3D12Drawer::phongInstancingNT( const IScene& scene, ID3D12GraphicsCommandList* pCmdList,
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle
) const {
    // æ¿¿Ã∂˚ «¡∑Œ≈‰ƒ› ∫Ò±≥
    assert(static_cast<const d3d12::CameraScene&>(scene).protocol() == protocol_);

    pCmdList->OMSetRenderTargets(1u, &rtvHandle, true, &dsvHandle);
    pCmdList->SetGraphicsRootShaderResourceView(0, resPerInstanceData_->GetGPUVirtualAddress());
    pCmdList->SetGraphicsRootShaderResourceView(1, resMaterials_->GetGPUVirtualAddress());
    pCmdList->SetGraphicsRootShaderResourceView(2, resLights_->GetGPUVirtualAddress());
    pCmdList->SetGraphicsRootConstantBufferView(3, resPerDrawcallData_->GetGPUVirtualAddress());
    pCmdList->SetGraphicsRootConstantBufferView(4, resPerFrameData_->GetGPUVirtualAddress());

    auto lightCnt = 0u;
    auto materialCnt = 0u;
    const d3d12::Mesh* pMesh = nullptr;
    auto instanceCnt = 0u;

    for (auto di : scene.iteration()) {
        auto type = di.get<rp::DIType>(rp::PhongInstancingNT::typeIdx);

        switch (type) {
        case rp::DIType::Light: {
            auto lights = di.get<std::span<rp::PhongInstancingNT::LightType>>(rp::PhongInstancingNT::lightIdx);

            for (const auto& light : lights) {
                pLights_[lightCnt++] = light;
                if (lightCnt == maxLights) {
                    break;
                }
            }
            break;
        }

        case rp::DIType::Material: {
            auto materials = di.get<std::span<rp::PhongInstancingNT::MaterialType>>(rp::PhongInstancingNT::materialIdx);

            for (const auto& material : materials) {
                pMats_[materialCnt++] = material;
                if (materialCnt == maxMaterials) {
                    break;
                }
            }
            break;
        }

        case rp::DIType::Mesh: {
            pMesh = di.get<const d3d12::Mesh*>(rp::PhongInstancingNT::meshIdx);
            pMesh->bind(pCmdList);
            break;
        }

        case rp::DIType::PID: {
            auto pids = di.get<std::span<rp::PhongInstancingNT::PIDType>>(rp::PhongInstancingNT::PDDIdx);

            for (const auto& pid : pids) {
                pPID_[instanceCnt++] = pid;
                if (instanceCnt == maxInstances) {
                    break;
                }
            }
            break;
        }

        case rp::DIType::PFD: {
            auto pfd = di.get<rp::PhongInstancingNT::PFDType>(rp::PhongInstancingNT::PFDIdx);

            *pPFD_ = pfd;
            break;
        }

        case rp::DIType::PDD: {
            auto pdd = di.get<rp::PhongInstancingNT::PDDType>(rp::PhongInstancingNT::PDDIdx);

            *pPDD_ = pdd;
            pPFD_->lightCnt = lightCnt;

            pMesh->draw(pCmdList, instanceCnt);
            break;
        }

        default:
            throw std::runtime_error("Invalid DrawInfo type.");
        };
    }
}

} // namespace gfx