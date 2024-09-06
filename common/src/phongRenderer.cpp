#include "phongRenderer.hpp"

#include "shaderPath.hpp"
#include "phongShader.hpp"
#include "d3d12Scene.hpp"
#include "d3d12Drawers.hpp"

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
    assert(dynamic_cast<d3d12::PhongShaderNT*>(&shader) != nullptr);
    d3d12::illuminanceDraw( scene, rp::PhongInstancingNT{},
        static_cast<d3d12::PhongShaderNT&>(shader), pCmdList
    );
}

void PhongRenderer::init(ICore& core) {
    if ( auto tmp = dynamic_cast<d3d12::Core*>(&core) ) {
        d3d12Drawer_.init(*this, static_cast<d3d12::Core&>(core));
        return;
    }
    
    throw GFX_EXCEPT("[Description] The Core is not supported.");
}

void PhongRenderer::D3D12Drawer::init(PhongRenderer& renderer, d3d12::Core& core) {
    if (core.containsShader(d3d12::PhongShader::shaderName())) {
        return;
    }

    core.addShader(d3d12::PhongShader::shaderName(), d3d12::PhongShader(core));
}

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

void PhongRenderer::D3D12Drawer::draw( const IScene& scene, d3d12::Shader& shader, ID3D12GraphicsCommandList* pCmdList,
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle
) const {
    assert(dynamic_cast<d3d12::PhongShader*>(&shader) != nullptr);
    d3d12::illuminanceDraw( scene, rp::PhongInstancing{},
        static_cast<d3d12::PhongShader&>(shader), pCmdList
    );
}

} // namespace gfx