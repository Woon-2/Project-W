#ifndef __D3D12_INPUT_LAYOUT_PRESETS_HPP
#define __D3D12_INPUT_LAYOUT_PRESETS_HPP

#include "inputLayoutPresets.hpp"
#include "d3d12core.hpp"

namespace gfx {

namespace d3d12 {

Core::InputLayoutIdx inputLayoutName(InputLayoutPreset preset);

}   // namespace d3d12

}   // namespace gfx

#endif // __D3D12_INPUT_LAYOUT_PRESETS_HPP