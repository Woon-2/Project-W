#include "d3d12shader.hpp"
#include <cassert>

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

    for (std::size_t ilIdx = 0; ilIdx < inputLayouts_.size(); ++ilIdx) {
        const auto& il = inputLayouts_[ilIdx];
        if (!psosMap_.contains(idx) || psosMap_.at(idx).size() <= ilIdx) {
            psosMap_[idx] = Cont<wrl::ComPtr<ID3D12PipelineState>>(inputLayouts_.size());
        }
        psoDesc.InputLayout = il;
        auto pPSO = wrl::ComPtr<ID3D12PipelineState>();
        DX_THROW_FAILED( pDevice->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &pPSO) );
        psosMap_[idx][ilIdx] = pPSO;
    }

    pRoot_ = desc.pRootSignature;
}

void Shader::bind(IRenderContext& ctx, std::any option) const {
    if (!pRoot_) {
        throw;  /*RootSignatureNotSet("The root signature is not set.");*/
    }

    const auto bo = std::any_cast<BindOption>(option);

    if (!psosMap_.contains(bo.idx)) {
        throw;  /*ShaderIdxNotFound("The shader index is not found.");*/
    }

    if (psosMap_.at(bo.idx).size() <= bo.ilIdx) {
        throw;  /*InputLayoutIdxNotFound("The input layout index is not found.");*/
    }

    if (!ctx.castableTo(gfx::RenderContextType::D3D12)) {
        throw;  /*InvalidRenderContext("The render context is not compatible with D3D12.");*/
    }

    auto pCmdList = std::any_cast< wrl::ComPtr<ID3D12GraphicsCommandList> >(
        ctx.cast(gfx::RenderContextType::D3D12)
    );

    pCmdList->SetGraphicsRootSignature(pRoot_.Get());
    pCmdList->SetPipelineState(psosMap_.at(bo.idx).at(bo.ilIdx).Get());
}

} // namespace d3d12

} // namespace gfx