#include "sampleRenderer.hpp"
#include "sampleScene.hpp"

#include "shaderPath.hpp"

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
    ).shader( SampleRenderer::shaderName() );

    shader.bind(pCmdList.Get(), 12345);

    D3D12Drawer::render(scene, pCmdList.Get(), pTarget, pDepthTarget);
}

void SampleRenderer::cleanup() {}

void SampleRenderer::D3D12Drawer::init(SampleRenderer& renderer, d3d12::Core& core) {
    if (core.containsShader(SampleRenderer::shaderName())) {
        return;
    }

    auto pRoot = core.root(&renderer);

    if (!pRoot) {
        throw;  /*RootSignatureNotFound("The root signature is not found.");*/
    }

    auto shader = d3d12::Shader();
    auto shaderBuilder = d3d12::SimpleShaderBuilder();
    shaderBuilder.setRootSignature(pRoot.Get());

    auto vsBlob = d3d12::Shader::loadCSO(compiledShaderPath/L"sampleShader_vs.cso");
    shaderBuilder.code( d3d12::Shader::Type::Vertex, D3D12_SHADER_BYTECODE{
        .pShaderBytecode = vsBlob->GetBufferPointer(),
        .BytecodeLength = vsBlob->GetBufferSize()
    } );
    auto psBlob = d3d12::Shader::loadCSO(compiledShaderPath/L"sampleShader_ps.cso");
    shaderBuilder.code( d3d12::Shader::Type::Pixel, D3D12_SHADER_BYTECODE{
        .pShaderBytecode = psBlob->GetBufferPointer(),
        .BytecodeLength = psBlob->GetBufferSize()
    } );

    D3D12_INPUT_ELEMENT_DESC ieDescs[] = {
        { "POSITION", 0u, DXGI_FORMAT_R32G32B32_FLOAT, 0u, 0u, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0u }
    };

    shaderBuilder.setInputLayout( D3D12_INPUT_LAYOUT_DESC{
        .pInputElementDescs = ieDescs,
        .NumElements = 1u
    } );

    shaderBuilder.build( static_cast<ID3D12Device*>( d3d12::DeviceFetcher::device(core) ), shader, 12345 );

    core.addShader(SampleRenderer::shaderName(), std::move(shader));
}

void SampleRenderer::D3D12Drawer::render( const IScene& scene, ID3D12GraphicsCommandList* pCmdList,
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle
) {
    pCmdList->OMSetRenderTargets(1u, &rtvHandle, true, &dsvHandle);

    while (auto di = scene.getDrawInfo()) {
        // TODO: decouple concrete draw info type with specific scene
        auto vb = di.value().get<D3D12_VERTEX_BUFFER_VIEW>(SampleScene::vbIdx);
        auto ib = di.value().get<D3D12_INDEX_BUFFER_VIEW>(SampleScene::ibIdx);

        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->IASetVertexBuffers(0u, 1u, &vb);
        pCmdList->IASetIndexBuffer(&ib);

        // TODO: keep track of the counts somewhere
        pCmdList->DrawIndexedInstanced(3u, 1u, 0u, 0u, 0u);
    }
}

} // namespace gfx