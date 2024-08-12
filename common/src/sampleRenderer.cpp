#include "sampleRenderer.hpp"
#include "d3d12Scene.hpp"

#include "shaderPath.hpp"
#include "solidShader.hpp"

namespace gfx {

void SampleRenderer::init(ICore& core) {
    if (auto tmp = dynamic_cast<d3d12::Core*>(&core); !tmp) {
        throw;  /*CoreTypeMismatch("The core type is mismatched.");*/
    }
    
    D3D12Drawer::init(*this, static_cast<d3d12::Core&>(core));
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

    D3D12Drawer::render(scene, pCmdList.Get(), pTarget, pDepthTarget);
}

void SampleRenderer::cleanup() {}

void SampleRenderer::D3D12Drawer::init(SampleRenderer& renderer, d3d12::Core& core) {
    if (core.containsShader(d3d12::SolidShader::shaderName())) {
        return;
    }

    core.addShader(d3d12::SolidShader::shaderName(), d3d12::SolidShader(core));
}

void SampleRenderer::D3D12Drawer::render( const IScene& scene, ID3D12GraphicsCommandList* pCmdList,
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle
) {
    pCmdList->OMSetRenderTargets(1u, &rtvHandle, true, &dsvHandle);

    for (auto di : scene.iteration()) {
        auto pMesh = di.get<const d3d12::Mesh*>(d3d12::CameraScene::meshIdx);
        auto world = di.get<const mu::Mat4x4>(d3d12::CameraScene::worldIdx);
        auto pView = di.get<const mu::Mat4x4*>(d3d12::CameraScene::viewIdx);
        auto pProj = di.get<const mu::Mat4x4*>(d3d12::CameraScene::projIdx);

        auto wvp = mu::transpose(world * (*pView) * (*pProj)).getXmf();
        auto color = mu::Vec4(1.f, 1.f, 0.f, 1.f).getXmf();
        // TODO: make root parameter setting more flexible
        pCmdList->SetGraphicsRoot32BitConstants(0, 16, &wvp, 0);
        pCmdList->SetGraphicsRoot32BitConstants(0, 4, &color, 16);

        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pMesh->bind(pCmdList);
        pMesh->draw(pCmdList);
    }
}

} // namespace gfx