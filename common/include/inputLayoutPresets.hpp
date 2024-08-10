#ifndef __INPUT_LAYOUT_PRESETS_HPP
#define __INPUT_LAYOUT_PRESETS_HPP

#include "inputLayout.hpp"

namespace gfx {

enum class InputLayoutPreset {
    Pos3
};

const InputLayout makeInputLayoutPreset(InputLayoutPreset preset);

}   // namespace gfx

#endif // __INPUT_LAYOUT_PRESETS_HPP