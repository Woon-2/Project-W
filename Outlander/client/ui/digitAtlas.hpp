#ifndef __digitAtlas_HPP
#define __digitAtlas_HPP

#include "../gfxUtil.hpp"   // Texture, XMFLOAT4, mu math types

class GFX;

// Renders integer numbers using a 10-cell (0..9) horizontal sprite atlas via
// the existing UIPipeline. Stateless helper shared by DamageNumberSystem and
// KillCountWidget so both use a single consistent digit look.
namespace DigitAtlas {

    // Atlas layout contract (must match resources/UI/damage_digits.dds):
    // 10 equal cells laid out horizontally; cell index == digit value.
    inline constexpr int   kCellCount  = 10;
    inline constexpr float kCellAspect = 64.0f / 96.0f;  // cell width / height

    enum class Align { Center, Left, Right };

    // Emits one UIPipeline::DrawEvent per digit of |value| (non-negative),
    // laid out horizontally. centerX/centerY are top-left-origin screen pixels
    // (same space as worldToScreen output); screenHeight flips Y into the UI
    // shader's bottom-origin space. colorMul tints the white-base glyphs.
    void emitNumber(GFX& gfx, const Texture* atlas,
                    float centerX, float centerY,
                    float glyphHeightPx, float screenHeight,
                    int value, const XMFLOAT4& colorMul,
                    Align align = Align::Center);

}  // namespace DigitAtlas

#endif  // __digitAtlas_HPP
