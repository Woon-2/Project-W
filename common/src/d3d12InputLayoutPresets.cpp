#include "d3d12InputLayoutPresets.hpp"

namespace gfx {

namespace d3d12 {

Core::InputLayoutIdx inputLayoutName(InputLayoutPreset preset) {
    switch (preset) {
    case InputLayoutPreset::Solid: return "Solid";
    case InputLayoutPreset::SolidDiffuse: return "SolidDiffuse";
    case InputLayoutPreset::S_SolidDiffuse: return "S_SolidDiffuse";
    case InputLayoutPreset::Color: return "Color";
    case InputLayoutPreset::S_Color: return "S_Color";
    case InputLayoutPreset::ColorDiffuse: return "ColorDiffuse";
    case InputLayoutPreset::S_ColorDiffuse: return "S_ColorDiffuse";
    case InputLayoutPreset::Tex: return "Tex";
    case InputLayoutPreset::S_Tex: return "S_Tex";
    case InputLayoutPreset::TexDiffuse: return "TexDiffuse";
    case InputLayoutPreset::S_TexDiffuse: return "S_TexDiffuse";
    default:
        throw GFX_EXCEPT("Invalid InputLayoutPreset");
    }
}

}   // namespace d3d12

}   // namespace gfx