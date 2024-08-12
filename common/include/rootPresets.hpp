#ifndef __ROOT_PRESETS_HPP
#define __ROOT_PRESETS_HPP

#include "d3d12core.hpp"

namespace gfx {

namespace d3d12 {

enum class RootPreset {
    Null,
    Solid
};

wrl::ComPtr<ID3D12RootSignature> makeRootPreset(Core& core, RootPreset preset);
Core::RootIdx rootName(RootPreset preset);

} // namespace d3d12

} // namespace gfx

#endif  // __ROOT_PRESETS_HPP