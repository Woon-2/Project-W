#include "pch.hpp"
#include "digitAtlas.hpp"
#include "../gfx.hpp"

namespace DigitAtlas {

void emitNumber(GFX& gfx, const Texture* atlas,
                float centerX, float centerY,
                float glyphHeightPx, float screenHeight,
                int value, const XMFLOAT4& colorMul, Align align)
{
    if (!atlas || !atlas->res) return;
    if (value < 0) value = -value;   // v1: damage / counts are non-negative

    // Decompose into digits (least-significant first), then render reversed.
    int digits[12];
    int n = 0;
    if (value == 0) {
        digits[n++] = 0;
    } else {
        while (value > 0 && n < 12) { digits[n++] = value % 10; value /= 10; }
    }

    const float gw = glyphHeightPx * kCellAspect;   // glyph cell width in pixels
    const float gh = glyphHeightPx;                 // glyph cell height in pixels

    // Horizontal center X of the first (most-significant) glyph.
    float firstCenterX;
    switch (align) {
        case Align::Left:  firstCenterX = centerX + 0.5f * gw;           break;
        case Align::Right: firstCenterX = centerX - (n - 0.5f) * gw;     break;
        case Align::Center:
        default:           firstCenterX = centerX - (n - 1) * 0.5f * gw; break;
    }

    const float cellUV = 1.0f / static_cast<float>(kCellCount);

    for (int i = 0; i < n; ++i) {
        const int   d  = digits[n - 1 - i];
        const float cx = firstCenterX + static_cast<float>(i) * gw;

        // Quad mesh spans [-1,1]; half-extents become the scale. Y is flipped
        // into the UI shader's bottom-origin space (see UIElement::buildWorldMatrix).
        const mu::Vec3 scl{ gw * 0.5f, gh * 0.5f, 1.f };
        const mu::Vec3 pos{ cx, screenHeight - centerY, 0.f };
        const mu::Mat4x4 world = mu::Mat4x4(mu::scale(scl)) * mu::translate(pos);

        gfx.addDrawEvent(UIPipeline::DrawEvent{
            .world       = world,
            .pTex        = atlas,
            .colorMul    = colorMul,
            .uvScaleBias = XMFLOAT4{ cellUV, 1.0f, static_cast<float>(d) * cellUV, 0.0f }
        });
    }
}

}  // namespace DigitAtlas
