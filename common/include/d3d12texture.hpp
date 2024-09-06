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
    Texture() NOEXCEPT
        : desc_{}, res_(), cpuHandle_{}, gpuHandle_{} {}

    void load(Core& core, const std::filesystem::path& path);

private:
    void loadDDS(Core& core, const std::filesystem::path& path);
    void loadWIC(Core& core, const std::filesystem::path& path);

    D3D12_RESOURCE_DESC desc_;
    wrl::ComPtr<ID3D12Resource> res_;

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle_;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle_;
};

}

}

#endif // __D3d12Texture_HPP