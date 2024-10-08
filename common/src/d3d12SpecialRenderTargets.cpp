#include "d3d12SpecialRendertargets.hpp"

namespace gfx {

namespace d3d12 {

bool ShadowTarget::castableTo(RenderTargetType type) const {
    return type == RenderTargetType::D3D12_DEPTH;
}

std::any ShadowTarget::cast(RenderTargetType type) {
    if (type != RenderTargetType::D3D12_DEPTH) {
        throw GFX_EXCEPT("Cannot cast to the requested render target type.");
    }

    return std::any{ pShadowTex_->dsv() };
}

void ShadowTarget::preRender(IRenderContext& renderContext) {
    if (!renderContext.castableTo(RenderContextType::D3D12)) {
        throw GFX_EXCEPT("The render context is not supported.");
    }

    auto pCmdList = std::any_cast<wrl::ComPtr<ID3D12GraphicsCommandList>>(
        renderContext.cast(RenderContextType::D3D12)
    );

    if (!pShadowTex_) {
        throw GFX_EXCEPT("The shadow texture is not set.");
    }

    if (pShadowTex_->state() == D3D12_RESOURCE_STATE_DEPTH_WRITE) {
        return;
    }

    pShadowTex_->cvt2ds(*static_cast<D3D12RenderContext*>(&renderContext));
    clear(renderContext);
}

void ShadowTarget::postRender(IRenderContext& renderContext) {
    if (!renderContext.castableTo(RenderContextType::D3D12)) {
        throw GFX_EXCEPT("The render context is not supported.");
    }

    auto pCmdList = std::any_cast<wrl::ComPtr<ID3D12GraphicsCommandList>>(
        renderContext.cast(RenderContextType::D3D12)
    );

    if (!pShadowTex_) {
        throw GFX_EXCEPT("The shadow texture is not set.");
    }

    if (pShadowTex_->state() == D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE) {
        return;
    }

    pShadowTex_->cvt2sr(*static_cast<D3D12RenderContext*>(&renderContext));
}

void ShadowTarget::clear(IRenderContext& renderContext) {
    if (!renderContext.castableTo(RenderContextType::D3D12)) {
        throw GFX_EXCEPT("The render context is not supported.");
    }

    auto pCmdList = std::any_cast<wrl::ComPtr<ID3D12GraphicsCommandList>>(
        renderContext.cast(RenderContextType::D3D12)
    );

    DX_THROW_FAILED_VOID( pCmdList->ClearDepthStencilView(
        pShadowTex_->dsv().cpuHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0u, 0, nullptr
    ) );
}

}   // namespace gfx::d3d12

}   // namespace gfx