#include "phongRenderer.hpp"
#include "d3d12Scene.hpp"

#include "shaderPath.hpp"
#include "phongShader.hpp"

namespace gfx {

void PhongRendererNT::init(ICore& core) {
    if ( auto tmp = dynamic_cast<d3d12::Core*>(&core) ) {
        d3d12Drawer_.init(*this, static_cast<d3d12::Core&>(core));
        return;
    }
    
    throw GFX_EXCEPT("[Description] The Core is not supported.");
}

// TODO: write the exception classes
void PhongRendererNT::render(const IScene& scene, IRenderContext& renderContext, IRenderTarget& target) const {
    assert(static_cast<const d3d12::CameraScene&>(scene).protocol() == protocol_);

    if ( renderContext.castableTo(RenderContextType::D3D12) ) {
        forwardToD3D12Drawer(scene, renderContext, target);
        return;
    }

    throw GFX_EXCEPT("[Description] The render context is not supported.");
}

void PhongRendererNT::forwardToD3D12Drawer( const IScene& scene, IRenderContext& renderContext,
    IRenderTarget& target
) const {
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
    d3d12Drawer_.draw(scene, shader, pCmdList.Get(), pTarget, pDepthTarget);
}

void PhongRendererNT::cleanup() {}

void PhongRendererNT::D3D12Drawer::init(PhongRendererNT& renderer, d3d12::Core& core) {
    if (core.containsShader(d3d12::PhongShaderNT::shaderName())) {
        return;
    }

    core.addShader(d3d12::PhongShaderNT::shaderName(), d3d12::PhongShaderNT(core));
}

void PhongRendererNT::D3D12Drawer::draw( const IScene& scene, d3d12::Shader& shader, ID3D12GraphicsCommandList* pCmdList,
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

void PhongRenderer::init(ICore& core) {
    if ( auto tmp = dynamic_cast<d3d12::Core*>(&core) ) {
        d3d12Drawer_.init(*this, static_cast<d3d12::Core&>(core));
        return;
    }
    
    throw GFX_EXCEPT("[Description] The Core is not supported.");
}

// TODO: write the exception classes
void PhongRenderer::render(const IScene& scene, IRenderContext& renderContext, IRenderTarget& target) const {
    assert(static_cast<const d3d12::CameraScene&>(scene).protocol() == protocol_);

    if ( renderContext.castableTo(RenderContextType::D3D12) ) {
        forwardToD3D12Drawer(scene, renderContext, target);
        return;
    }

    throw GFX_EXCEPT("[Description] The render context is not supported.");
}

void PhongRenderer::forwardToD3D12Drawer( const IScene& scene, IRenderContext& renderContext,
    IRenderTarget& target
) const {
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
    ).shader( d3d12::PhongShader::shaderName() );

    shader.bind(pCmdList.Get(), 0);
    d3d12Drawer_.draw(scene, shader, pCmdList.Get(), pTarget, pDepthTarget);
}

void PhongRenderer::cleanup() {}

void PhongRenderer::D3D12Drawer::init(PhongRenderer& renderer, d3d12::Core& core) {
    if (core.containsShader(d3d12::PhongShader::shaderName())) {
        return;
    }

    core.addShader(d3d12::PhongShader::shaderName(), d3d12::PhongShader(core));
}

void PhongRenderer::D3D12Drawer::draw( const IScene& scene, d3d12::Shader& shader, ID3D12GraphicsCommandList* pCmdList,
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle
) const {
    assert(static_cast<const d3d12::CameraScene&>(scene).protocol() == protocol_);
    assert(dynamic_cast<d3d12::PhongShader*>(&shader) != nullptr);

    using Light = rp::PhongInstancing::LightType;
    using Material = rp::PhongInstancing::MaterialType;
    using PID = rp::PhongInstancing::PIDType;
    using PFD = rp::PhongInstancing::PFDType;
    using PDD = rp::PhongInstancing::PDDType;

    auto pShader = static_cast<d3d12::PhongShader*>(&shader);

    pCmdList->OMSetRenderTargets(1u, &rtvHandle, true, &dsvHandle);
    pCmdList->SetGraphicsRootShaderResourceView(0, pShader->pid().gpuAddress());
    pCmdList->SetGraphicsRootShaderResourceView(1, pShader->materials().gpuAddress());
    pCmdList->SetGraphicsRootDescriptorTable(2, pShader->texSrvStart());
    pCmdList->SetGraphicsRootShaderResourceView(3, pShader->lights().gpuAddress());
    pCmdList->SetGraphicsRootConstantBufferView(4, pShader->pdd().gpuAddress());
    pCmdList->SetGraphicsRootConstantBufferView(5, pShader->pfd().gpuAddress());

    auto lightCnt = 0ull;
    auto materialCnt = 0ull;
    const d3d12::Mesh* pMesh = nullptr;
    auto instanceCnt = 0ull;
    auto pfd = PFD{};

    for (auto di : scene.iteration()) {
        auto type = di.get<rp::DIType>(rp::PhongInstancing::typeIdx);

        switch (type) {
        case rp::DIType::Light: {
            auto lights = di.get<std::span<Light>>(rp::PhongInstancing::lightIdx);
            auto cnt = std::min(d3d12::PhongShaderNT::defMaxLights - lightCnt, lights.size());
            pShader->lights().uploadRegion( lights.data(), cnt * sizeof(Light), lightCnt * sizeof(Light) );
            lightCnt += cnt;
            break;
        }

        case rp::DIType::Material: {
            auto materials = di.get<std::span<Material>>(rp::PhongInstancing::materialIdx);
            auto cnt = std::min(d3d12::PhongShaderNT::defMaxMaterials - materialCnt, materials.size());
            pShader->materials().uploadRegion(materials.data(), cnt * sizeof(Material), materialCnt * sizeof(Material));
            materialCnt += cnt;
            break;
        }

        case rp::DIType::Mesh: {
            pMesh = di.get<const d3d12::Mesh*>(rp::PhongInstancing::meshIdx);
            pMesh->bind(pCmdList);
            break;
        }

        case rp::DIType::PID: {
            auto pids = di.get<std::span<PID>>(rp::PhongInstancing::PIDIdx);
            auto cnt = std::min(d3d12::PhongShaderNT::defMaxInstances - instanceCnt, pids.size());
            pShader->pid().uploadRegion(pids.data(), cnt * sizeof(PID), instanceCnt * sizeof(PID));
            instanceCnt += cnt;
            break;
        }

        case rp::DIType::PFD: {
            pfd = di.get<PFD>(rp::PhongInstancing::PFDIdx);
            break;
        }

        case rp::DIType::PDD: {
            auto pdd = di.get<PDD>(rp::PhongInstancing::PDDIdx);
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