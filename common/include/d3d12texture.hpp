#ifndef __D3d12Texture_HPP
#define __D3d12Texture_HPP

#include "d3d12core.hpp"

#include "texLoader/DDSTextureLoader12.h"
#include "texLoader/WICTextureLoader12.h"

#include <filesystem>

namespace gfx {

namespace d3d12 {

class Texture {
public:
    static const Core::DescHeapIdx texSrvHeapIdx;

    Texture() NOEXCEPT
        : desc_{}, res_(), gpuHandle_{} {}

    Texture( Core& core, D3D12RenderContext& ctx, const std::filesystem::path& path,
        Core::UpBufIdx upIdx,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
    ) : Texture() {
        load(core, ctx, path, upIdx, initialState);
    }

    void load( Core& core, D3D12RenderContext& ctx, const std::filesystem::path& path,
        Core::UpBufIdx upIdx,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
    );

    void completeInit(Core& core) const {
        core.popTmpUpBuf(upIdx_);
    }

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle() const NOEXCEPT { return gpuHandle_; }

private:
    void loadDDS( Core& core, D3D12RenderContext& ctx, const std::filesystem::path& path,
        Core::UpBufIdx&& upIdx, D3D12_RESOURCE_STATES initialState
    );
    void loadWIC( Core& core, D3D12RenderContext& ctx, const std::filesystem::path& path,
        Core::UpBufIdx&& upIdx, D3D12_RESOURCE_STATES initialState
    );
    
    D3D12_RESOURCE_DESC desc_;
    wrl::ComPtr<ID3D12Resource> res_;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle_;
    Core::UpBufIdx upIdx_;
};

}

}

#endif // __D3d12Texture_HPP