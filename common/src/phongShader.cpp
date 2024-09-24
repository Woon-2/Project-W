#include "phongShader.hpp"

#include "d3d12texture.hpp"

#include <ranges>

namespace gfx {

namespace d3d12 {

PhongShaderNT::PhongShaderNT(Core& core, const Config& config, std::size_t duplicationCnt)
    : Shader(),
    internalResArr_(4, std::ranges::range_value_t<decltype(internalResArr_)>(duplicationCnt)),
    resPerFrameData_(core, sizeof(d3d12::sr::BasicPFD) , internalResArr_[0], duplicationCnt),
    resPerDrawcallData_(core, sizeof(d3d12::sr::PDDNTPhong), internalResArr_[1], duplicationCnt),
    resPerInstanceData_(core, sizeof(d3d12::sr::BasicPID), internalResArr_[2], duplicationCnt),
    resLights_(core, sizeof(d3d12::sr::PhongLight), internalResArr_[3], duplicationCnt),
    maxInstances_(config.maxInstances),
    maxLights_(config.maxLights), frameIdx_(0u) {
    auto builder = SimpleShaderBuilder();
    builder.code(Type::Vertex, loadCSO(compiledShaderPath / "ntShader_vs.cso"));
    builder.code(Type::Pixel, loadCSO(compiledShaderPath / "ntShader_ps.cso"));

    auto pRoot = core.root(rootName());
    auto pDevice = static_cast<ID3D12Device*>(DeviceFetcher::device(core));

    const auto& il = core.inputLayout(inputLayoutName());

    builder.setInputLayout(il).setRoot(pRoot).build(pDevice, *this, 0u);
    builder.wireframe().build(pDevice, *this, 1u);
}

void PhongShaderNT::setRootParams(ID3D12GraphicsCommandList* pCmdList) const {
    DX_THROW_FAILED_VOID( pCmdList->SetGraphicsRootShaderResourceView(
        0u, resPerInstanceData_.gpuAddress(frameIdx_)
    ) );
    DX_THROW_FAILED_VOID( pCmdList->SetGraphicsRootShaderResourceView(
        1u, resLights_.gpuAddress(frameIdx_)
    ) );
    DX_THROW_FAILED_VOID( pCmdList->SetGraphicsRootConstantBufferView(
        2u, resPerDrawcallData_.gpuAddress(frameIdx_)
    ) );
    DX_THROW_FAILED_VOID( pCmdList->SetGraphicsRootConstantBufferView(
        3u, resPerFrameData_.gpuAddress(frameIdx_)
    ) );
}

PhongShader::PhongShader(Core &core, const Config &config, std::size_t duplicationCnt) 
    : Shader(),
    internalResArr_(4, std::ranges::range_value_t<decltype(internalResArr_)>(duplicationCnt)),
    resPerFrameData_(core, sizeof(d3d12::sr::BasicPFD), internalResArr_[0], duplicationCnt),
    resPerDrawcallData_(core, sizeof(d3d12::sr::PDDPhong), internalResArr_[1], duplicationCnt),
    resPerInstanceData_(core, sizeof(d3d12::sr::BasicPID), internalResArr_[2], duplicationCnt),
    resLights_(core, sizeof(d3d12::sr::PhongLight), internalResArr_[3], duplicationCnt),
    texSrvStart_(),
    pTexSrvHeap_(nullptr),
    maxInstances_(config.maxInstances),
    maxLights_(config.maxLights) {

    if ( !core.containsDescHeap(Texture::texSrvHeapIdx) ) {
        throw GFX_EXCEPT("[Description] Texture descriptor heap not found.");
    }

    texSrvStart_ = core.descHeap(Texture::texSrvHeapIdx).gpuHandleUninit();
    pTexSrvHeap_ = &core.descHeap(Texture::texSrvHeapIdx);

    auto builder = SimpleShaderBuilder();
    builder.code(Type::Vertex, loadCSO(compiledShaderPath / "tShader_vs.cso"));
    builder.code(Type::Pixel, loadCSO(compiledShaderPath / "tShader_ps.cso"));

    auto pRoot = core.root(rootName());
    auto pDevice = static_cast<ID3D12Device*>(DeviceFetcher::device(core));

    const auto& il = core.inputLayout(inputLayoutName());

    builder.setInputLayout(il).setRoot(pRoot).build(pDevice, *this, 0u);
    builder.wireframe().build(pDevice, *this, 1u);
}

void PhongShader::setRootParams(ID3D12GraphicsCommandList* pCmdList, size_t frameIdx) const {
    // temporary
    pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pTexSrvHeap_->set(pCmdList);

    DX_THROW_FAILED_VOID( pCmdList->SetGraphicsRootShaderResourceView(
        0u, resPerInstanceData_.gpuAddress(frameIdx)
    ) );
    DX_THROW_FAILED_VOID( pCmdList->SetGraphicsRootDescriptorTable(
        1u, texSrvStart_
    ) );
    DX_THROW_FAILED_VOID( pCmdList->SetGraphicsRootShaderResourceView(
        2u, resLights_.gpuAddress(frameIdx)
    ) );
    DX_THROW_FAILED_VOID( pCmdList->SetGraphicsRootConstantBufferView(
        3u, resPerDrawcallData_.gpuAddress(frameIdx)
    ) );
    DX_THROW_FAILED_VOID( pCmdList->SetGraphicsRootConstantBufferView(
        4u, resPerFrameData_.gpuAddress(frameIdx)
    ) );
}

}   // namespace gfx::d3d12

}   // namespace gfx
