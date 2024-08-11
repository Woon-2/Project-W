#include "sampleRenderer.hpp"
#include "sampleScene.hpp"

#include "shaderPath.hpp"
#include "nullShader.hpp"

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
    ).shader( d3d12::NullShader::shaderName() );

    shader.bind(pCmdList.Get(), 0);

    D3D12Drawer::render(scene, pCmdList.Get(), pTarget, pDepthTarget);
}

void SampleRenderer::cleanup() {}

void SampleRenderer::D3D12Drawer::init(SampleRenderer& renderer, d3d12::Core& core) {
    if (core.containsShader(d3d12::NullShader::shaderName())) {
        return;
    }

    core.addShader(d3d12::NullShader::shaderName(), d3d12::NullShader(core));
}

void SampleRenderer::D3D12Drawer::render( const IScene& scene, ID3D12GraphicsCommandList* pCmdList,
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle
) {
    pCmdList->OMSetRenderTargets(1u, &rtvHandle, true, &dsvHandle);

    for (auto di : scene.iteration()) {
        auto pMesh = di.get<const d3d12::Mesh*>(SampleScene::meshIdx);

        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pMesh->bind(pCmdList);
        pMesh->draw(pCmdList);
    }
}

} // namespace gfx