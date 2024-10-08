#include "d3d12Descriptor.hpp"

namespace gfx {

namespace d3d12 {

void Descriptor::makeSrv( ID3D12Device* pDevice, ID3D12Resource* pRes,
    const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
) {
    DX_THROW_FAILED_VOID( pDevice->CreateShaderResourceView(
        pRes, &srvDesc, cpuHandle_
    ) );
}

void Descriptor::makeSrv(ID3D12Device* pDevice, ID3D12Resource* pRes) {
    DX_THROW_FAILED_VOID( pDevice->CreateShaderResourceView(
        pRes, nullptr, cpuHandle_
    ) );
}

void Descriptor::makeRtv( ID3D12Device* pDevice, ID3D12Resource* pRes,
    const D3D12_RENDER_TARGET_VIEW_DESC& rtvDesc
) {
    DX_THROW_FAILED_VOID( pDevice->CreateRenderTargetView(
        pRes, &rtvDesc, cpuHandle_
    ) );
}

void Descriptor::makeRtv(ID3D12Device* pDevice, ID3D12Resource* pRes) {
    DX_THROW_FAILED_VOID( pDevice->CreateRenderTargetView(
        pRes, nullptr, cpuHandle_
    ) );
}

void Descriptor::makeDsv( ID3D12Device* pDevice, ID3D12Resource* pRes,
    const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc
) {
    DX_THROW_FAILED_VOID( pDevice->CreateDepthStencilView(
        pRes, &dsvDesc, cpuHandle_
    ) );
}

void Descriptor::makeDsv(ID3D12Device* pDevice, ID3D12Resource* pRes) {
    DX_THROW_FAILED_VOID( pDevice->CreateDepthStencilView(
        pRes, nullptr, cpuHandle_
    ) );
}

void Descriptor::makeUav( ID3D12Device* pDevice, ID3D12Resource* pRes,
    const D3D12_UNORDERED_ACCESS_VIEW_DESC& uavDesc
) {
    DX_THROW_FAILED_VOID( pDevice->CreateUnorderedAccessView(
        pRes, nullptr, &uavDesc, cpuHandle_
    ) );
}

void Descriptor::makeCbv( ID3D12Device* pDevice,
    const D3D12_CONSTANT_BUFFER_VIEW_DESC& cbvDesc
) {
    DX_THROW_FAILED_VOID( pDevice->CreateConstantBufferView(
        &cbvDesc, cpuHandle_
    ) );
}

void Descriptor::makeSam(ID3D12Device* pDevice, const D3D12_SAMPLER_DESC& samplerDesc) {
    DX_THROW_FAILED_VOID( pDevice->CreateSampler(
        &samplerDesc, cpuHandle_
    ) );
}

DescriptorHeap::DescriptorHeap( ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type,
    std::size_t capacity, bool shaderVisible
) : pHeap_(), descriptors_(capacity), cpuStart_(), gpuStart_(),
    stride_{}, type_(type), shaderVisible_(shaderVisible) {
    auto desc = D3D12_DESCRIPTOR_HEAP_DESC{
        .Type = type,
        .NumDescriptors = static_cast<UINT>(capacity),
        .Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0
    };

    DX_THROW_FAILED( pDevice->CreateDescriptorHeap(
        &desc, __uuidof(ID3D12DescriptorHeap), &pHeap_
    ) );

    stride_ = pDevice->GetDescriptorHandleIncrementSize(type);
    cpuStart_ = pHeap_->GetCPUDescriptorHandleForHeapStart();
    if (shaderVisible) {
        gpuStart_ = pHeap_->GetGPUDescriptorHandleForHeapStart();
    }

    auto cpuHandle = cpuStart_;
    auto gpuHandle = gpuStart_;

    for (std::size_t i = 0; i < capacity; ++i) {
        descriptors_[i] = Descriptor(cpuHandle, gpuHandle, i, Descriptor::Type::NUL, shaderVisible, false);
        cpuHandle.ptr += stride_;
        gpuHandle.ptr += stride_;
    }
}

}   // namespace gfx::d3d12

}   // namespace gfx