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

    case InputLayoutPreset::Pos3Norm3:
        return InputLayout( 24u,
            InputLayout::Element{
                .prop = Vertex::Properties::Position,
                .offset = 0u,
            },
            InputLayout::Element{
                .prop = Vertex::Properties::Normal,
                .offset = 12u,
            }
        );

    default:
        throw GFX_EXCEPT("Invalid InputLayoutPreset");
    }
}

}   // namespace gfx