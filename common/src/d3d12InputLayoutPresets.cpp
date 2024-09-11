#include "d3d12InputLayoutPresets.hpp"

namespace gfx {

namespace d3d12 {

void configInputLayoutAux(Vertex::Properties prop) {
    switch (prop) {
    case Vertex::Properties::Position:
        InputLayout::configPropertyAux( Vertex::Properties::Position,
            "POSITION", 0u, DXGI_FORMAT_R32G32B32_FLOAT, 0u
        );
        break;

    case Vertex::Properties::Normal:
        InputLayout::configPropertyAux( Vertex::Properties::Normal,
            "NORMAL", 0u, DXGI_FORMAT_R32G32B32_FLOAT, 0u
        );
        break;

    case Vertex::Properties::TexCoord:
        InputLayout::configPropertyAux( Vertex::Properties::Normal,
            "TEX", 0u, DXGI_FORMAT_R32G32_FLOAT, 0u
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

    case InputLayoutPreset::Pos3Norm3:
        return "Pos3Norm3";

    case InputLayoutPreset::Pos3Norm3Tex2:
        return "Pos3Norm3Tex2";

    default:
        throw GFX_EXCEPT("Invalid InputLayoutPreset");
    }
}

}   // namespace d3d12

}   // namespace gfx