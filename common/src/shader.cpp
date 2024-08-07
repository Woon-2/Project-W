#include "shader.hpp"

namespace gfx {

namespace d3d12 {

void Shader::make(Core& core, Idx idx, const Desc& desc) {
    auto pDevice = static_cast<ID3D12Device*>( DeviceFetcher::device(core) );

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
}

} // namespace d3d12

} // namespace gfx