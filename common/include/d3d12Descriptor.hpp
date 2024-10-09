#ifndef __D3D12DESCRIPTOR_HPP
#define __D3D12DESCRIPTOR_HPP

#include <directx/d3dx12.h>
#include <directx/d3d12.h>

#include "dxtarget.hpp"
#include "dxexcept.hpp"

#include <vector>

namespace gfx {

namespace d3d12 {

class Descriptor {
public:
    friend class DescriptorHeap;

    enum class Type {
        SRV, RTV, DSV, UAV, CBV, SAM, NUL
    };

    Descriptor()
        : cpuHandle_(), gpuHandle_(), idx_(std::size_t(-1)),
        type_(Type::NUL), shaderVisible_(false), initialized_(false) {}

    Type type() const NOEXCEPT {
        return type_;
    }

    bool shaderVisible() const NOEXCEPT {
        return shaderVisible_;
    }

    bool initialized() const NOEXCEPT {
        return initialized_;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle() NOEXCEPT {
        return cpuHandle_;
    }

    const D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle() const NOEXCEPT {
        return cpuHandle_;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle() NOEXCEPT {
        return gpuHandle_;
    }

    const D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle() const NOEXCEPT {
        return gpuHandle_;
    }

    std::size_t idx() const NOEXCEPT {
        return idx_;
    }

    void makeSrv(ID3D12Device* pDevice, ID3D12Resource* pRes, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc);
    void makeSrv(ID3D12Device* pDevice, ID3D12Resource* pRes);
    void makeRtv(ID3D12Device* pDevice, ID3D12Resource* pRes, const D3D12_RENDER_TARGET_VIEW_DESC& rtvDesc);
    void makeRtv(ID3D12Device* pDevice, ID3D12Resource* pRes);
    void makeDsv(ID3D12Device* pDevice, ID3D12Resource* pRes, const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc);
    void makeDsv(ID3D12Device* pDevice, ID3D12Resource* pRes);
    void makeUav(ID3D12Device* pDevice, ID3D12Resource* pRes, const D3D12_UNORDERED_ACCESS_VIEW_DESC& uavDesc);
    void makeCbv(ID3D12Device* pDevice, const D3D12_CONSTANT_BUFFER_VIEW_DESC& cbvDesc);
    void makeSam(ID3D12Device* pDevice, const D3D12_SAMPLER_DESC& samplerDesc);

private:
    Descriptor( const D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle,
        const D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle, std::size_t idx, Type type,
        bool shaderVisible, bool initialized
    ) : cpuHandle_(cpuHandle), gpuHandle_(gpuHandle), idx_(idx), type_(type),
        shaderVisible_(shaderVisible), initialized_(initialized) {}

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle_;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle_;
    std::size_t idx_;
    Type type_;
    bool shaderVisible_;
    bool initialized_;
};

class DescriptorHeap {
public:
    DescriptorHeap()
        : pHeap_(), descriptors_(), cpuStart_(), gpuStart_(), stride_(0),
        type_(D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES), shaderVisible_(false) {}
    DescriptorHeap(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, std::size_t capacity, bool shaderVisible = false);

    Descriptor& at(std::size_t idx) {
        return descriptors_.at(idx);
    }

    const Descriptor& at(std::size_t idx) const {
        return descriptors_.at(idx);
    }

    Descriptor& operator[](std::size_t idx) NOEXCEPT {
        return descriptors_[idx];
    }

    const Descriptor& operator[](std::size_t idx) const NOEXCEPT {
        return descriptors_[idx];
    }

    void set(ID3D12GraphicsCommandList* pCmdList) {
        DX_THROW_FAILED_VOID(pCmdList->SetDescriptorHeaps(1u, pHeap_.GetAddressOf()));
    }

    std::size_t capacity() const NOEXCEPT {
        return descriptors_.capacity();
    }

    std::size_t stride() const NOEXCEPT {
        return stride_;
    }

    D3D12_DESCRIPTOR_HEAP_TYPE type() const NOEXCEPT {
        return type_;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE& cpuStart() NOEXCEPT {
        return cpuStart_;
    }

    const D3D12_CPU_DESCRIPTOR_HANDLE& cpuStart() const NOEXCEPT {
        return cpuStart_;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE& gpuStart() NOEXCEPT {
        return gpuStart_;
    }

    const D3D12_GPU_DESCRIPTOR_HANDLE& gpuStart() const NOEXCEPT {
        return gpuStart_;
    }

    bool shaderVisible() const NOEXCEPT {
        return shaderVisible_;
    }

    auto begin() NOEXCEPT {
        return descriptors_.begin();
    }

    auto end() NOEXCEPT {
        return descriptors_.end();
    }

    auto begin() const NOEXCEPT {
        return descriptors_.begin();
    }

    auto end() const NOEXCEPT {
        return descriptors_.end();
    }

    auto cbegin() const NOEXCEPT {
        return descriptors_.cbegin();
    }

    auto cend() const NOEXCEPT {
        return descriptors_.cend();
    }

    ID3D12DescriptorHeap* get() NOEXCEPT {
        return pHeap_.Get();
    }

    const ID3D12DescriptorHeap* get() const NOEXCEPT {
        return pHeap_.Get();
    }

private:
    wrl::ComPtr<ID3D12DescriptorHeap> pHeap_;
    std::vector<Descriptor> descriptors_;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuStart_;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuStart_;
    std::size_t stride_;
    D3D12_DESCRIPTOR_HEAP_TYPE type_;
    bool shaderVisible_;
};

}   // namespace gfx::d3d12

}   // namespace gfx

#endif // __D3D12DESCRIPTOR_HPP