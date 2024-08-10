#include "inputLayoutPresets.hpp"

#include "gfxExcept.hpp"

namespace gfx {

const InputLayout makeInputLayoutPreset(InputLayoutPreset preset) {
    switch (preset) {
    case InputLayoutPreset::Pos3:
        return InputLayout( 12u,
            InputLayout::Element{
                .prop = Vertex::Properties::Position,
                .offset = 0u,
            }
        );

    default:
        throw GFX_EXCEPT("Invalid InputLayoutPreset");
    }
}

}   // namespace gfx