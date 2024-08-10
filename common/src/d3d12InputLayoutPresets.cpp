#include "d3d12InputLayoutPresets.hpp"

namespace gfx {

namespace d3d12 {

void configInputLayoutAux(InputLayoutPreset preset) {
    switch (preset) {
    case InputLayoutPreset::Pos3:
        InputLayout::configPropertyAux( Vertex::Properties::Position,
            "POSITION", 0u, DXGI_FORMAT_R32G32B32_FLOAT, 0u
        );
        break;
    default:
        throw GFX_EXCEPT("Invalid InputLayoutPreset");
    }
}

Core::InputLayoutIdx inputLayoutName(InputLayoutPreset preset) {
    switch (preset) {
    case InputLayoutPreset::Pos3:
        return "Pos3";
    default:
        throw GFX_EXCEPT("Invalid InputLayoutPreset");
    }
}

}   // namespace d3d12

}   // namespace gfx