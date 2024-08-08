#include "d3d12shader.hpp"

namespace gfx {

namespace d3d12 {

void Shader::make(ID3D12Device* pDevice, Idx idx, const Desc& desc) {
    auto psoDesc = D3D12_GRAPHICS_PIPELINE_STATE_DESC{
        .pRootSignature = desc.pRootSignature,
        .VS = codes_.at(Type::Vertex),
        .PS = codes_.at(Type::Pixel),
        .BlendState = desc.blend,
        .SampleMask = desc.sampleMask,
        .RasterizerState = desc.RasterizerState,
        .DepthStencilState = desc.DepthStencilState,
        .InputLayout = desc.InputLayout,
        .IBStripCutValue = desc.IBStripCutValue,
        .PrimitiveTopologyType = desc.PrimitiveTopologyType,
        .NumRenderTargets = desc.NumRenderTargets,
        .DSVFormat = desc.DSVFormat,
        .SampleDesc = desc.SampleDesc,
        .NodeMask = desc.NodeMask,
        .CachedPSO = desc.CachedPSO,
        .Flags = desc.Flags
    };

    for (UINT i = 0; i < desc.NumRenderTargets; ++i) {
        psoDesc.RTVFormats[i] = desc.RTVFormats[i];
    }

    auto pPSO = wrl::ComPtr<ID3D12PipelineState>();
    DX_THROW_FAILED( pDevice->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &pPSO) );

    psos_[idx] = pPSO;
}

void Shader::bind(ID3D12GraphicsCommandList* pCmdList, Idx idx) const {
    if (!psos_.contains(idx)) {
        throw;  /*ShaderIdxNotFound("The shader index is not found.");*/
    }

    pCmdList->SetPipelineState(psos_.at(idx).Get());
}

} // namespace d3d12

} // namespace gfx