#include "d3d12shader.hpp"

namespace gfx {

namespace d3d12 {

void Shader::make(ID3D12Device* pDevice, Idx idx, const Desc& desc) {
    auto vsBlob = codes_.at(Type::Vertex);
    auto psBlob = codes_.at(Type::Pixel);

    auto psoDesc = D3D12_GRAPHICS_PIPELINE_STATE_DESC{
        .pRootSignature = desc.pRootSignature.Get(),
        .VS = D3D12_SHADER_BYTECODE{
            .pShaderBytecode = vsBlob->GetBufferPointer(),
            .BytecodeLength = vsBlob->GetBufferSize()
        },
        .PS = D3D12_SHADER_BYTECODE{
            .pShaderBytecode = psBlob->GetBufferPointer(),
            .BytecodeLength = psBlob->GetBufferSize()
        },
        .BlendState = desc.blend,
        .SampleMask = desc.sampleMask,
        .RasterizerState = desc.rasterizerState,
        .DepthStencilState = desc.depthStencilState,
        .InputLayout = desc.inputLayout,
        .IBStripCutValue = desc.ibStripCutValue,
        .PrimitiveTopologyType = desc.primitiveTopologyType,
        .NumRenderTargets = desc.numRenderTargets,
        .DSVFormat = desc.dsvFormat,
        .SampleDesc = desc.sampleDesc,
        .NodeMask = desc.nodeMask,
        .CachedPSO = desc.cachedPSO,
        .Flags = desc.flags
    };

    for (UINT i = 0; i < desc.numRenderTargets; ++i) {
        psoDesc.RTVFormats[i] = desc.rtvFormats[i];
    }

    auto pPSO = wrl::ComPtr<ID3D12PipelineState>();
    DX_THROW_FAILED( pDevice->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &pPSO) );

    psos_[idx] = pPSO;

    pRoot_ = desc.pRootSignature;
    inputLayout_ = std::move( desc.inputLayout );
}

void Shader::bind(ID3D12GraphicsCommandList* pCmdList, Idx idx) const {
    if (!pRoot_) {
        throw;  /*RootSignatureNotSet("The root signature is not set.");*/
    }

    if (!psos_.contains(idx)) {
        throw;  /*ShaderIdxNotFound("The shader index is not found.");*/
    }

    pCmdList->SetGraphicsRootSignature(pRoot_.Get());
    pCmdList->SetPipelineState(psos_.at(idx).Get());
}

} // namespace d3d12

} // namespace gfx