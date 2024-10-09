#ifndef __INPUT_LAYOUT_PRESETS_HPP
#define __INPUT_LAYOUT_PRESETS_HPP

#include "inputLayout.hpp"

namespace gfx {

enum class InputLayoutPreset {
    Solid,
    SolidDiffuse,
    S_SolidDiffuse,
    Color,
    S_Color,
    ColorDiffuse,
    S_ColorDiffuse,
    Tex,
    S_Tex,
    TexDiffuse,
    S_TexDiffuse
};

const InputLayout makeInputLayoutPreset(InputLayoutPreset preset);

}   // namespace gfx

#endif // __INPUT_LAYOUT_PRESETS_HPP