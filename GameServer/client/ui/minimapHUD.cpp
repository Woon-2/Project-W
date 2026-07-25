#include "pch.hpp"
#include "minimapHUD.hpp"
#include "../gfx.hpp"
#include "../uiPipeline.hpp"
#include "uiShapes.hpp"

#include <algorithm>
#include <cmath>

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

// Base (pre-uiScale) icon sizes. Monsters are small so dense packs don't flood the map.
float iconBasePx(MinimapEntityIcon::Kind kind) {
    switch (kind) {
    case MinimapEntityIcon::Kind::Boss:    return 8.0f;
    case MinimapEntityIcon::Kind::Self:    return 6.5f;
    case MinimapEntityIcon::Kind::Party:   return 5.0f;
    case MinimapEntityIcon::Kind::Monster:
    default:                               return 3.5f;
    }
}

}   // anonymous namespace

void MinimapHUD::zoomIn()  { zoom_ = std::min(zoom_ * kZoomStep, kZoomMax); }
void MinimapHUD::zoomOut() { zoom_ = std::max(zoom_ / kZoomStep, kZoomMin); }

void MinimapHUD::render(GFX& gfx, mu::Vec3 playerWorldPos,
                         mu::Vec3 bakedCenter, float bakedCoverage,
                         const std::vector<MinimapEntityIcon>& entities,
                         float screenW, float screenH,
                         const MinimapGuide& guide) const
{
    if (!visible_) return;

    const float scaleX  = (kBaseWidth  > 0.f) ? screenW / kBaseWidth  : 1.f;
    const float scaleY  = (kBaseHeight > 0.f) ? screenH / kBaseHeight : 1.f;
    const float uiScale = std::max(0.01f, std::min(scaleX, scaleY));

    const float sizePx   = baseSizePx_   * uiScale;
    const float borderPx = baseBorderPx_ * uiScale;
    const float halfPx   = sizePx * 0.5f;

    // Top-right placement (where the removed Hi-Z debug print used to sit).
    const float pivotX = screenW - (screenW * marginXFrac_ + halfPx);
    // Bottom-origin pixel space (matches the UI shader): top placement subtracts from screenH.
    const float pivotY = screenH - (screenH * marginYFrac_ + halfPx);

    // Effective world-space view half-extent (zoom). Clamp so the sampled sub-rect stays
    // within the baked coverage (avoids sampling clamped/garbage outside the cache).
    float viewRadius = baseViewRadius_ / zoom_;
    const float maxRadius = (bakedCoverage > 0.f) ? bakedCoverage * 0.45f : viewRadius;
    viewRadius = std::clamp(viewRadius, 5.f, std::max(5.f, maxRadius));

    // Frame (dark border) drawn first so the background sits on top of it.
    gfx.addDrawEvent(UIPipeline::DrawEvent{
        .world    = quadWorld(pivotX, pivotY, sizePx + borderPx * 2.f),
        .pTex     = gfx.solidColorTex(),
        .colorMul = XMFLOAT4{ 0.05f, 0.05f, 0.06f, 0.9f }
    });

    // Cached terrain background, scrolled via a UV sub-rect centered on the player.
    const Texture* bgTex = gfx.minimapTextureForThisFrame();
    if (bgTex && bakedCoverage > 0.f) {
        const float cov = bakedCoverage;
        const float H   = cov * 0.5f;
        const float px  = playerWorldPos.x();
        const float pz  = playerWorldPos.z();
        const float cx  = bakedCenter.x();
        const float cz  = bakedCenter.z();

        // uv' = uv * scale + bias. U: west->east (+X). V: north(+Z) at top (small V).
        const XMFLOAT4 uvScaleBias{
            (2.f * viewRadius) / cov,                 // scaleU
            (2.f * viewRadius) / cov,                 // scaleV
            (px - viewRadius - cx + H) / cov,         // biasU (west edge)
            ((cz + H) - (pz + viewRadius)) / cov      // biasV (north edge)
        };

        gfx.addDrawEvent(UIPipeline::DrawEvent{
            .world       = quadWorld(pivotX, pivotY, sizePx),
            .pTex        = bgTex,
            .colorMul    = XMFLOAT4{ 1.f, 1.f, 1.f, 1.f },
            .uvScaleBias = uvScaleBias
        });
    }

    // Guidance route polyline (under the entity icons). North-up player-relative
    // projection, same mapping as the icons; only segments fully inside the map are
    // drawn so the line never spills past the frame (the edge arrow covers beyond).
    const XMFLOAT4 guideCol{ 0.42f, 0.90f, 1.f, 0.92f };
    if (guide.active && guide.polyline.size() >= 2u) {
        auto toMap = [&](const mu::Vec3& wp, float& x, float& y, bool& inside) {
            const float u = (wp.x() - playerWorldPos.x()) / viewRadius;
            const float v = (wp.z() - playerWorldPos.z()) / viewRadius;
            x = pivotX + u * halfPx;
            y = pivotY + v * halfPx;
            inside = (u > -0.98f && u < 0.98f && v > -0.98f && v < 0.98f);
        };
        for (std::size_t i = 0; i + 1u < guide.polyline.size(); ++i) {
            float ax, ay, bx, by; bool ain, bin;
            toMap(guide.polyline[i],      ax, ay, ain);
            toMap(guide.polyline[i + 1u], bx, by, bin);
            if (!ain || !bin) continue;
            const float dx = bx - ax, dy = by - ay;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len < 0.5f) continue;
            uiShapes::quad(gfx, gfx.solidColorTex(), (ax + bx) * 0.5f, (ay + by) * 0.5f,
                           len, 2.2f * uiScale, std::atan2(dy, dx), guideCol);
        }
    }

    // Entity icons: player-relative North-up projection at the current view radius.
    for (const auto& e : entities) {
        const float localU = (e.worldPos.x() - playerWorldPos.x()) / viewRadius;
        const float localV = (e.worldPos.z() - playerWorldPos.z()) / viewRadius;  // +Z(north) = up
        if (localU < -1.f || localU > 1.f || localV < -1.f || localV > 1.f) continue;  // outside: cull

        const float iconX = pivotX + localU * halfPx;
        const float iconY = pivotY + localV * halfPx;  // bottom-origin: north(+Z) -> higher Y -> top

        gfx.addDrawEvent(UIPipeline::DrawEvent{
            .world    = quadWorld(iconX, iconY, iconBasePx(e.kind) * uiScale),
            .pTex     = gfx.solidColorTex(),
            .colorMul = iconColor(e.kind)
        });
    }

    // Edge arrow toward the guidance look-ahead when it lies outside the view radius.
    if (guide.active) {
        float tu = (guide.target.x() - playerWorldPos.x()) / viewRadius;
        float tv = (guide.target.z() - playerWorldPos.z()) / viewRadius;
        if (std::fabs(tu) > 1.f || std::fabs(tv) > 1.f) {
            float len = std::sqrt(tu * tu + tv * tv);
            if (len < 1e-4f) { tu = 0.f; tv = 1.f; len = 1.f; }
            tu /= len; tv /= len;
            const float inset = 0.9f;
            const float sc    = inset / std::max(std::fabs(tu), std::fabs(tv));
            const float ax    = pivotX + tu * sc * halfPx;
            const float ay    = pivotY + tv * sc * halfPx;
            uiShapes::arrow(gfx, gfx.solidColorTex(), ax, ay,
                            16.f * uiScale, std::atan2(tv, tu),
                            XMFLOAT4{ 0.72f, 0.97f, 1.f, 1.f });
        }
    }
}
