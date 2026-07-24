#include "pch.hpp"
#include "uiShapes.hpp"

#include "../gfx.hpp"
#include "../uiPipeline.hpp"

#include <cmath>

namespace uiShapes {

void quad(GFX& gfx, const Texture* tex, float cx, float cy,
          float w, float h, float angleRad, const XMFLOAT4& col)
{
    // scale about origin -> rotate -> translate to the pixel centre (row-vector chain).
    const mu::Mat4x4 world =
          mu::Mat4x4(mu::scale(mu::Vec3{ w * 0.5f, h * 0.5f, 1.f }))
        * mu::rotateZH(mu::Radian(angleRad))
        * mu::translate(mu::Vec3{ cx, cy, 0.f });

    gfx.addDrawEvent(UIPipeline::DrawEvent{
        .world    = world,
        .pTex     = tex,
        .colorMul = col,
    });
}

void diamond(GFX& gfx, const Texture* tex, float cx, float cy,
             float sizePx, const XMFLOAT4& col)
{
    constexpr float kQuarterTurn = 0.7853982f;  // 45 degrees
    quad(gfx, tex, cx, cy, sizePx, sizePx, kQuarterTurn, col);
}

void arrow(GFX& gfx, const Texture* tex, float cx, float cy,
           float sizePx, float angleRad, const XMFLOAT4& col)
{
    const float s  = sizePx;
    const float ca = std::cos(angleRad);
    const float sa = std::sin(angleRad);
    const float th = 0.20f * s;   // stroke thickness

    // Place a bar whose centre is given in arrow-local space (x forward, y left) with
    // an extra local rotation; the whole arrow then rotates as a unit by angleRad.
    auto bar = [&](float lx, float ly, float len, float localRot) {
        const float wx = cx + lx * ca - ly * sa;
        const float wy = cy + lx * sa + ly * ca;
        quad(gfx, tex, wx, wy, len, th, angleRad + localRot, col);
    };

    constexpr float kBarb = 2.3561945f;   // 135 degrees

    bar(-0.02f * s,  0.00f * s, 0.66f * s,  0.f);      // shaft
    bar( 0.26f * s,  0.15f * s, 0.44f * s,  kBarb);    // upper barb
    bar( 0.26f * s, -0.15f * s, 0.44f * s, -kBarb);    // lower barb
}

}  // namespace uiShapes
