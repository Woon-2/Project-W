#include "inputLayoutPresets.hpp"

#include "gfxExcept.hpp"

namespace gfx {

namespace {
    const InputLayout ilSolid() {
        InputLayout il;
        il.configSlots(InputLayout::SlotIdx(1u));
        il.configProperty(Vertex::Properties::Position3D, 0u);
        return il;
    }

    const InputLayout ilSolidDiffuse() {
        InputLayout il;
        il.configSlots(InputLayout::SlotIdx(2u));
        il.configProperty(Vertex::Properties::Position3D, InputLayout::SlotIdx(0u));
        il.configProperty(Vertex::Properties::Normal3D, InputLayout::SlotIdx(1u));
        return il;
    }

    const InputLayout silSolidDiffuse() {
        InputLayout il;
        il.configSlots(InputLayout::SlotIdx(1u));
        il.configProperty(Vertex::Properties::Position3D, InputLayout::SlotIdx(0u));
        il.configProperty(Vertex::Properties::Normal3D, InputLayout::SlotIdx(0u));
        return il;
    }

    const InputLayout ilColor() {
        InputLayout il;
        il.configSlots(InputLayout::SlotIdx(2u));
        il.configProperty(Vertex::Properties::Position3D, InputLayout::SlotIdx(0u));
        il.configProperty(Vertex::Properties::Color4D, InputLayout::SlotIdx(1u));
        return il;
    }

    const InputLayout silColor() {
        InputLayout il;
        il.configSlots(InputLayout::SlotIdx(1u));
        il.configProperty(Vertex::Properties::Position3D, InputLayout::SlotIdx(0u));
        il.configProperty(Vertex::Properties::Color4D, InputLayout::SlotIdx(0u));
        return il;
    }

    const InputLayout ilColorDiffuse() {
        InputLayout il;
        il.configSlots(InputLayout::SlotIdx(3u));
        il.configProperty(Vertex::Properties::Position3D, InputLayout::SlotIdx(0u));
        il.configProperty(Vertex::Properties::Normal3D, InputLayout::SlotIdx(1u));
        il.configProperty(Vertex::Properties::Color4D, InputLayout::SlotIdx(2u));
        return il;
    }

    const InputLayout silColorDiffuse() {
        InputLayout il;
        il.configSlots(InputLayout::SlotIdx(1u));
        il.configProperty(Vertex::Properties::Position3D, InputLayout::SlotIdx(0u));
        il.configProperty(Vertex::Properties::Normal3D, InputLayout::SlotIdx(0u));
        il.configProperty(Vertex::Properties::Color4D, InputLayout::SlotIdx(0u));
        return il;
    }

    const InputLayout ilTex() {
        InputLayout il;
        il.configSlots(InputLayout::SlotIdx(2u));
        il.configProperty(Vertex::Properties::Position3D, InputLayout::SlotIdx(0u));
        il.configProperty(Vertex::Properties::TexCoord2D0, InputLayout::SlotIdx(1u));
        return il;
    }

    const InputLayout silTex() {
        InputLayout il;
        il.configSlots(InputLayout::SlotIdx(1u));
        il.configProperty(Vertex::Properties::Position3D, InputLayout::SlotIdx(0u));
        il.configProperty(Vertex::Properties::TexCoord2D0, InputLayout::SlotIdx(0u));
        return il;
    }

    const InputLayout ilTexDiffuse() {
        InputLayout il;
        il.configSlots(InputLayout::SlotIdx(3u));
        il.configProperty(Vertex::Properties::Position3D, InputLayout::SlotIdx(0u));
        il.configProperty(Vertex::Properties::Normal3D, InputLayout::SlotIdx(1u));
        il.configProperty(Vertex::Properties::TexCoord2D0, InputLayout::SlotIdx(2u));
        return il;
    }

    const InputLayout silTexDiffuse() {
        InputLayout il;
        il.configSlots(InputLayout::SlotIdx(1u));
        il.configProperty(Vertex::Properties::Position3D, InputLayout::SlotIdx(0u));
        il.configProperty(Vertex::Properties::Normal3D, InputLayout::SlotIdx(0u));
        il.configProperty(Vertex::Properties::TexCoord2D0, InputLayout::SlotIdx(0u));
        return il;
    }

}   // namespace gfx::{anonymous_namespace}

const InputLayout makeInputLayoutPreset(InputLayoutPreset preset) {
    switch (preset) {
    case InputLayoutPreset::Solid: return ilSolid();
    case InputLayoutPreset::SolidDiffuse: return ilSolidDiffuse();
    case InputLayoutPreset::S_SolidDiffuse: return silSolidDiffuse();
    case InputLayoutPreset::Color: return ilColor();
    case InputLayoutPreset::S_Color: return silColor();
    case InputLayoutPreset::ColorDiffuse: return ilColorDiffuse();
    case InputLayoutPreset::S_ColorDiffuse: return silColorDiffuse();
    case InputLayoutPreset::Tex: return ilTex();
    case InputLayoutPreset::S_Tex: return silTex();
    case InputLayoutPreset::TexDiffuse: return ilTexDiffuse();
    case InputLayoutPreset::S_TexDiffuse: return silTexDiffuse();
    
    default:
        throw GFX_EXCEPT("Invalid InputLayoutPreset");
    }
}

}   // namespace gfx