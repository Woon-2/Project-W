#ifndef __ROOT_PRESETS_HPP
#define __ROOT_PRESETS_HPP

#include "d3d12core.hpp"

namespace gfx {

namespace d3d12 {

enum class RootPreset {
    Null
};

wrl::ComPtr<ID3D12RootSignature> makeRootPreset(ID3D12Device* pDevice, RootPreset preset);

} // namespace d3d12

} // namespace gfx

#endif  // __ROOT_PRESETS_HPP