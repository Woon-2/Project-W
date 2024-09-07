#include "phongShader.hpp"

#include "d3d12texture.hpp"

#include <ranges>

namespace gfx {

namespace d3d12 {

PhongShaderNT::PhongShaderNT(Core& core, const Config& config, std::size_t duplicationCnt)
    : Shader(),
    internalResArr_(5, std::ranges::range_value_t<decltype(internalResArr_)>(duplicationCnt)),
    resPerFrameData_(core, sizeof(d3d12::sr::BasicPFD) , internalResArr_[0]),
    resPerDrawcallData_(core, sizeof(d3d12::sr::BasicPDD), internalResArr_[1]),
    resPerInstanceData_(core, sizeof(d3d12::sr::BasicPID), internalResArr_[2]),
    resMaterials_(core, sizeof(d3d12::sr::PhongMaterialNT), internalResArr_[3]),
    resLights_(core, sizeof(d3d12::sr::PhongLight), internalResArr_[4]),
    maxInstances_(config.maxInstances),
    maxMaterials_(config.maxMaterials),
    maxLights_(config.maxLights) {
    auto builder = SimpleShaderBuilder();
    builder.code(Type::Vertex, loadCSO(compiledShaderPath / "ntShader_vs.cso"));
    builder.code(Type::Pixel, loadCSO(compiledShaderPath / "ntShader_ps.cso"));

    auto pRoot = core.root(rootName());
    auto pDevice = static_cast<ID3D12Device*>(DeviceFetcher::device(core));

    const auto& il = core.inputLayout(inputLayoutName());

    builder.setInputLayout(il).setRoot(pRoot).build(pDevice, *this, 0u);
    builder.wireframe().build(pDevice, *this, 1u);
}

void PhongShaderNT::setRootParams(ID3D12GraphicsCommandList* pCmdList, size_t frameIdx) const {
    DX_THROW_FAILED_VOID( pCmdList->SetGraphicsRootShaderResourceView(
        0, resPerInstanceData_.gpuAddress(frameIdx)
    ) );
    DX_THROW_FAILED_VOID( pCmdList->SetGraphicsRootShaderResourceView(
        1, resMaterials_.gpuAddress(frameIdx)
    ) );
    DX_THROW_FAILED_VOID( pCmdList->SetGraphicsRootShaderResourceView(
        2, resLights_.gpuAddress(frameIdx)
    ) );
    DX_THROW_FAILED_VOID( pCmdList->SetGraphicsRootConstantBufferView(
        3, resPerDrawcallData_.gpuAddress(frameIdx)
    ) );
    DX_THROW_FAILED_VOID( pCmdList->SetGraphicsRootConstantBufferView(
        4, resPerFrameData_.gpuAddress(frameIdx)
    ) );
}

PhongShader::PhongShader(Core &core, const Config &config, std::size_t duplicationCnt) 
    : Shader(),
    internalResArr_(5, std::ranges::range_value_t<decltype(internalResArr_)>(duplicationCnt)),
    resPerFrameData_(core, sizeof(d3d12::sr::BasicPFD), internalResArr_[0]),
    resPerDrawcallData_(core, sizeof(d3d12::sr::BasicPDD), internalResArr_[1]),
    resPerInstanceData_(core, sizeof(d3d12::sr::BasicPID), internalResArr_[2]),
    resMaterials_(core, sizeof(d3d12::sr::PhongMaterial), internalResArr_[3]),
    resLights_(core, sizeof(d3d12::sr::PhongLight), internalResArr_[4]),
    texSrvStart_(),
    maxInstances_(config.maxInstances),
    maxMaterials_(config.maxMaterials),
    maxLights_(config.maxLights) {

    if ( !core.containsDescHeap(Texture::texSrvHeapIdx) ) {
        throw GFX_EXCEPT("[Description] Texture descriptor heap not found.");
    }

    texSrvStart_ = core.descHeap(Texture::texSrvHeapIdx).gpuHandle();

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
    DX_THROW_FAILED_VOID( pCmdList->SetGraphicsRootShaderResourceView(
        0, resPerInstanceData_.gpuAddress(frameIdx)
    ) );
    DX_THROW_FAILED_VOID( pCmdList->SetGraphicsRootShaderResourceView(
        1, resMaterials_.gpuAddress(frameIdx)
    ) );
    DX_THROW_FAILED_VOID( pCmdList->SetGraphicsRootDescriptorTable(
        2, texSrvStart_
    ) );
    DX_THROW_FAILED_VOID( pCmdList->SetGraphicsRootShaderResourceView(
        3, resLights_.gpuAddress(frameIdx)
    ) );
    DX_THROW_FAILED_VOID( pCmdList->SetGraphicsRootConstantBufferView(
        4, resPerDrawcallData_.gpuAddress(frameIdx)
    ) );
    DX_THROW_FAILED_VOID( pCmdList->SetGraphicsRootConstantBufferView(
        5, resPerFrameData_.gpuAddress(frameIdx)
    ) );
}

}   // namespace gfx::d3d12

}   // namespace gfx
