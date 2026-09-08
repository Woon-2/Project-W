#include "pch.hpp"
#include "pathGuideHUD.hpp"

#include "../gfx.hpp"
#include "../uiPipeline.hpp"
#include "uiShapes.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace {

// Guidance palette (matches the ribbon's HDR cyan, clamped for the LDR UI pass).
const XMFLOAT4 kCore  { 0.55f, 0.95f, 1.00f, 1.00f };
const XMFLOAT4 kGlow  { 0.35f, 0.85f, 1.00f, 0.38f };
const XMFLOAT4 kWhite { 0.90f, 1.00f, 1.00f, 1.00f };
const XMFLOAT4 kArrow { 0.60f, 0.95f, 1.00f, 1.00f };
const XMFLOAT4 kDigit { 0.72f, 0.97f, 1.00f, 1.00f };

}  // namespace

void PathGuideHUD::init(GFX& gfx) {
    // A small offscreen target for the "<n>m" label (re-rasterised on metre change).
    gfx.createTextImageImmediate(256u, 64u, &distText_);
    textReady_ = true;
    lastDist_  = -1;
}

void PathGuideHUD::drawDistanceLabel(GFX& gfx, float cx, float cy, int meters, float uiScale) {
    if (!textReady_) return;

    if (meters != lastDist_) {
        WCHAR buf[32] = {};
        const int n = swprintf_s(buf, L"%dm", meters);
        if (n > 0) {
            std::fill(distText_.pData.begin(), distText_.pData.end(), static_cast<BYTE>(0));
            int tw = 0, th = 0;
            if (gfx.WriteTextToBitmap(&distText_, distText_.width, distText_.height,
                                      distText_.width * 4u, &tw, &th, nullptr,
                                      buf, static_cast<DWORD>(n))) {
                gfx.UpdateTextureWithTextImage(&distText_, distText_.width, distText_.height);
                textW_    = static_cast<float>(tw);
                textH_    = static_cast<float>(th);
                lastDist_ = meters;
            }
        }
    }
    if (textW_ <= 0.f || textH_ <= 0.f) return;

    // Draw only the rasterised sub-rect (top-left of the target), sized to a fixed
    // label height and centred at (cx,cy). No rotation -- always upright & readable.
    const float qH = 17.f * uiScale;
    const float qW = qH * (textW_ / textH_);
    const mu::Mat4x4 world =
          mu::Mat4x4(mu::scale(mu::Vec3{ qW * 0.5f, qH * 0.5f, 1.f }))
        * mu::translate(mu::Vec3{ cx, cy, 0.f });

    gfx.addDrawEvent(UIPipeline::DrawEvent{
        .world       = world,
        .pTex        = &distText_.texture,
        .pCopySrc    = &distText_.textureUpload,
        .colorMul    = kDigit,
        .uvScaleBias = XMFLOAT4{ textW_ / static_cast<float>(distText_.width),
                                 textH_ / static_cast<float>(distText_.height), 0.f, 0.f },
    });
}

void PathGuideHUD::render(GFX& gfx, const mu::Mat4x4& view, const mu::Mat4x4& proj,
                          mu::Vec3 targetWorld, float distMeters, bool active,
                          float screenW, float screenH)
{
    if (!visible_ || !active || screenW <= 0.f || screenH <= 0.f) return;

    const float scaleX  = screenW / kBaseWidth;
    const float scaleY  = screenH / kBaseHeight;
    const float uiScale = std::max(0.5f, std::min(scaleX, scaleY));

    const Texture* solid = gfx.solidColorTex();
    const int meters     = std::max(0, static_cast<int>(std::lround(distMeters)));

    // Gentle pulse (stateless clock; the beacon holds no per-frame state).
    static const auto t0 = std::chrono::steady_clock::now();
    const float t     = std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count();
    const float pulse = 0.5f + 0.5f * std::sin(t * 3.0f);

    // Project the look-ahead point to clip space (same transform as worldToScreen,
    // but we keep clip.w so off-screen / behind-camera cases can still be pointed at).
    const mu::Vec4 clip = mu::Vec4(targetWorld, 1.f) * (view * proj);
    const float w      = clip.w();
    const bool  behind = (w <= 1e-4f);
    const float ndcX   = behind ? 0.f : clip.x() / w;
    const float ndcY   = behind ? 0.f : clip.y() / w;

    // Bottom-origin pixels (y up), matching uiShapes / the UI shader.
    auto toPix = [&](float nx, float ny, float& px, float& py) {
        px = (nx + 1.f) * 0.5f * screenW;
        py = (ny + 1.f) * 0.5f * screenH;
    };

    const float insetOn = 0.94f;
    const bool onScreen  = !behind
                        && std::fabs(ndcX) <= insetOn && std::fabs(ndcY) <= insetOn;

    if (onScreen) {
        float px, py;
        toPix(ndcX, ndcY, px, py);
        uiShapes::diamond(gfx, solid, px, py, 34.f * uiScale * (1.f + 0.12f * pulse), kGlow);
        uiShapes::diamond(gfx, solid, px, py, 20.f * uiScale, kCore);
        uiShapes::diamond(gfx, solid, px, py,  8.f * uiScale, kWhite);
        drawDistanceLabel(gfx, px, py - 26.f * uiScale, meters, uiScale);  // below the beacon
        return;
    }

    // Off-screen / behind: point an arrow from the target direction, clamped to a
    // screen-edge inset rectangle. When behind, mirror the clip-space direction.
    float dx = behind ? -clip.x() : ndcX;
    float dy = behind ? -clip.y() : ndcY;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-4f) { dx = 0.f; dy = -1.f; len = 1.f; }
    dx /= len; dy /= len;

    const float m     = 0.86f;   // edge inset in NDC
    const float scale = m / std::max(std::fabs(dx), std::fabs(dy));
    float px, py;
    toPix(dx * scale, dy * scale, px, py);
    const float ang = std::atan2(dy, dx);

    uiShapes::arrow(gfx, solid, px, py, 42.f * uiScale, ang, kGlow);
    uiShapes::arrow(gfx, solid, px, py, 34.f * uiScale, ang, kArrow);
    // Distance nudged toward the screen interior so it never clips off the edge.
    drawDistanceLabel(gfx, px - dx * 30.f * uiScale, py - dy * 30.f * uiScale, meters, uiScale);
}
