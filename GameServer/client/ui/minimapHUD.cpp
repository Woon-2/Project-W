#include "pch.hpp"
#include "minimapHUD.hpp"
#include "../gfx.hpp"
#include "../uiPipeline.hpp"

namespace {

inline mu::Mat4x4 quadWorld(float cx, float cy, float size) {
    const float h = size * 0.5f;
    return mu::Mat4x4(mu::scale(mu::Vec3{ h, h, 1.f }))
         * mu::translate(mu::Vec3{ cx, cy, 0.f });
}

XMFLOAT4 iconColor(MinimapEntityIcon::Kind kind) {
    switch (kind) {
    case MinimapEntityIcon::Kind::Self:    return XMFLOAT4{ 0.f,  1.f,  0.f,  1.f };  // green
    case MinimapEntityIcon::Kind::Party:   return XMFLOAT4{ 0.f,  0.4f, 1.f,  1.f };  // blue
    case MinimapEntityIcon::Kind::Boss:    return XMFLOAT4{ 1.f,  0.55f,0.f,  1.f };  // orange
    case MinimapEntityIcon::Kind::Monster:
    default:                               return XMFLOAT4{ 1.f,  0.f,  0.f,  1.f };  // red
    }
}

float iconSizePx(MinimapEntityIcon::Kind kind) {
    return (kind == MinimapEntityIcon::Kind::Boss) ? 9.f
         : (kind == MinimapEntityIcon::Kind::Self) ? 8.f
         : 6.f;
}

}   // anonymous namespace

void MinimapHUD::render(GFX& gfx, mu::Vec3 playerWorldPos, float worldRadius,
                         const std::vector<MinimapEntityIcon>& entities,
                         float screenW, float screenH) const
{
    if (!visible_ || worldRadius <= 0.f) return;

    const float pivotX = screenW * marginXFrac_ + sizePx_ * 0.5f;
    // Bottom-origin pixel space (matches the UI shader) — top placement subtracts from screenH.
    const float pivotY = screenH - (screenH * marginYFrac_ + sizePx_ * 0.5f);

    // Frame (dark border quad, drawn first so the background sits on top of it).
    gfx.addDrawEvent(UIPipeline::DrawEvent{
        .world    = quadWorld(pivotX, pivotY, sizePx_ + borderPx_ * 2.f),
        .pTex     = gfx.solidColorTex(),
        .colorMul = XMFLOAT4{ 0.05f, 0.05f, 0.06f, 0.9f }
    });

    // Cached terrain background.
    const Texture* bgTex = gfx.minimapTextureForThisFrame();
    if (bgTex) {
        gfx.addDrawEvent(UIPipeline::DrawEvent{
            .world    = quadWorld(pivotX, pivotY, sizePx_),
            .pTex     = bgTex,
            .colorMul = XMFLOAT4{ 1.f, 1.f, 1.f, 1.f }
        });
    }

    // Entity icons: North-up world-XZ -> local offset -> screen pixel. No 3D rendering.
    for (const auto& e : entities) {
        const float dx = e.worldPos.x() - playerWorldPos.x();
        const float dz = e.worldPos.z() - playerWorldPos.z();
        const float localU =  dx / worldRadius;
        const float localV = -dz / worldRadius;   // +Z (north) maps to "up" on screen
        if (localU < -1.f || localU > 1.f || localV < -1.f || localV > 1.f) continue;   // outside the map: cull

        const float iconX = pivotX + localU * (sizePx_ * 0.5f);
        const float iconY = pivotY - localV * (sizePx_ * 0.5f);   // screen-up = +pivotY in bottom-origin space

        gfx.addDrawEvent(UIPipeline::DrawEvent{
            .world    = quadWorld(iconX, iconY, iconSizePx(e.kind)),
            .pTex     = gfx.solidColorTex(),
            .colorMul = iconColor(e.kind)
        });
    }
}
