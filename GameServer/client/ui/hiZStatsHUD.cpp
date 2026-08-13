#include "pch.hpp"
#include "hiZStatsHUD.hpp"

#include "../uiPipeline.hpp"
#include "uiShapes.hpp"

#include <algorithm>
#include <cstdio>

namespace {

// Panel geometry in base-resolution pixels (1024x768), top-down origin.
constexpr float kLeftPx      = 20.f;    // matches the party HP column inset
constexpr float kTopPx       = 380.f;   // just below the last party HP row (ends at ~353)
constexpr float kMinWidthPx  = 352.f;
constexpr float kPadPx       = 9.f;
constexpr float kRowPitchPx  = 18.f;
constexpr float kFontSizePx  = 13.f;
constexpr float kBarHeightPx = 9.f;
constexpr float kBarLeftFrac = 0.40f;   // bar starts here, the "culled x%" label sits left of it

// The overlay is a diagnostic panel, not gameplay UI: past 2x the text stops needing
// to grow and the 1024px-wide raster targets would start clipping long rows.
constexpr float kMaxSizeScale = 2.0f;
constexpr float kMinSizeScale = 0.75f;

const XMFLOAT4 kPlate     { 0.02f, 0.03f, 0.05f, 0.66f };
const XMFLOAT4 kTitleOn   { 0.55f, 1.00f, 0.60f, 1.00f };
const XMFLOAT4 kTitleOff  { 0.62f, 0.64f, 0.70f, 1.00f };
const XMFLOAT4 kHeader    { 0.62f, 0.70f, 0.82f, 1.00f };
const XMFLOAT4 kValue     { 0.94f, 0.96f, 1.00f, 1.00f };
const XMFLOAT4 kFootnote  { 0.72f, 0.76f, 0.84f, 1.00f };
const XMFLOAT4 kBarTrack  { 1.00f, 1.00f, 1.00f, 0.14f };

// Green when little is culled, amber when a lot is: the bar reads as "how much work
// the culling stages removed this frame".
XMFLOAT4 barFillColor(float ratio) {
    const float t = std::clamp(ratio, 0.f, 1.f);
    return XMFLOAT4{ 0.35f + 0.60f * t, 0.90f - 0.18f * t, 0.35f - 0.20f * t, 0.92f };
}

}  // namespace

void HiZStatsHUD::init(GFX& gfx) {
    if (ready_) return;   // setupStage() runs again on re-entry; keep the live targets
    for (auto& img : rows_)
        gfx.createTextImageImmediate(1024u, 64u, &img);
    ready_ = true;
}

void HiZStatsHUD::ensureFont(GFX& gfx, float sizeScale) {
    if (fontScale_ == sizeScale) return;
    // Monospace, so the three numeric columns line up from plain space padding.
    font_      = gfx.createFont(L"Consolas", kFontSizePx * sizeScale, DWRITE_FONT_WEIGHT_REGULAR);
    fontScale_ = sizeScale;
    // Glyph metrics changed: force every row to re-rasterise.
    for (auto& c : cache_) c.clear();
}

void HiZStatsHUD::rebuildText(const GFX::HiZStats& avg, bool hiZEnabled,
                              u32t objSkipped, u32t objTracked, float fps)
{
    WCHAR buf[128] = {};

    swprintf_s(buf, L"%-34s%10s", L"Hi-Z Occlusion Culling",
               hiZEnabled ? L"[H] ON" : L"[H] OFF");
    text_[RowTitle] = buf;

    swprintf_s(buf, L"%-14s%10s%10s%10s", L"", L"Skinned", L"Props", L"Total");
    text_[RowHeader] = buf;

    swprintf_s(buf, L"%-14s%10u%10u%10u", L"No culling",
               avg.skinnedSubmitted, avg.propSubmitted,
               avg.skinnedSubmitted + avg.propSubmitted);
    text_[RowNoCull] = buf;

    swprintf_s(buf, L"%-14s%10u%10u%10u", L"+ Frustum",
               avg.skinnedAfterFrustum, avg.propAfterFrustum,
               avg.skinnedAfterFrustum + avg.propAfterFrustum);
    text_[RowFrustum] = buf;

    swprintf_s(buf, L"%-14s%10u%10u%10u", L"+ Hi-Z",
               avg.skinnedAfterHiZ, avg.propAfterHiZ,
               avg.skinnedAfterHiZ + avg.propAfterHiZ);
    text_[RowHiZ] = buf;

    // Culled fraction of everything that was submitted, measured at the last stage
    // that is actually running.
    const u32t submitted = avg.skinnedSubmitted + avg.propSubmitted;
    const u32t remaining = hiZEnabled ? (avg.skinnedAfterHiZ + avg.propAfterHiZ)
                                      : (avg.skinnedAfterFrustum + avg.propAfterFrustum);
    culledRatio_ = (submitted > 0u)
        ? std::clamp(1.f - static_cast<float>(remaining) / static_cast<float>(submitted), 0.f, 1.f)
        : 0.f;
    swprintf_s(buf, L"culled %5.1f%%", culledRatio_ * 100.f);
    text_[RowRatio] = buf;

    swprintf_s(buf, L"Anim/Physics skipped %u / %u objects", objSkipped, objTracked);
    text_[RowSkip] = buf;

    swprintf_s(buf, L"%.1f FPS", fps);
    text_[RowFps] = buf;
}

void HiZStatsHUD::drawRow(GFX& gfx, int r, float leftPx, float cy,
                          float glyphH, const XMFLOAT4& color)
{
    auto& img = rows_[r];
    if (text_[r].empty()) return;

    if (text_[r] != cache_[r]) {
        std::fill(img.pData.begin(), img.pData.end(), static_cast<BYTE>(0));
        int tw = 0, th = 0;
        if (gfx.WriteTextToBitmap(&img, img.width, img.height, img.width * 4u,
                                  &tw, &th, &font_,
                                  text_[r].c_str(), static_cast<DWORD>(text_[r].size()))) {
            gfx.UpdateTextureWithTextImage(&img, img.width, img.height);
            measW_[r] = static_cast<float>(tw);
            measH_[r] = static_cast<float>(th);
            cache_[r] = text_[r];
            needsCopy_[r] = true;
        }
    }
    if (measW_[r] <= 0.f || measH_[r] <= 0.f) return;

    // 1:1 with the rasterised pixels (the font was already built at this scale), so
    // the glyphs stay crisp instead of being resampled.
    const float qH = std::min(measH_[r], glyphH);
    const float qW = measW_[r] * (qH / measH_[r]);
    const mu::Mat4x4 world =
          mu::Mat4x4(mu::scale(mu::Vec3{ qW * 0.5f, qH * 0.5f, 1.f }))
        * mu::translate(mu::Vec3{ leftPx + qW * 0.5f, cy, 0.f });

    gfx.addDrawEvent(UIPipeline::DrawEvent{
        .world       = world,
        .pTex        = &img.texture,
        .pCopySrc    = needsCopy_[r] ? &img.textureUpload : nullptr,
        .colorMul    = color,
        .uvScaleBias = XMFLOAT4{ measW_[r] / static_cast<float>(img.width),
                                 measH_[r] / static_cast<float>(img.height), 0.f, 0.f },
    });
    needsCopy_[r] = false;
}

void HiZStatsHUD::render(GFX& gfx, const GFX::HiZStats& stats, bool hiZEnabled,
                         u32t objSkipped, u32t objTracked, float fps,
                         float screenW, float screenH)
{
    if (!visible_ || !ready_ || screenW <= 0.f || screenH <= 0.f) return;

    const float uiScale   = std::max(0.5f, std::min(screenW / kBaseWidth, screenH / kBaseHeight));
    const float sizeScale = std::clamp(uiScale, kMinSizeScale, kMaxSizeScale);
    ensureFont(gfx, sizeScale);

    // Toggling Hi-Z changes what the numbers mean, so drop the window and refresh now.
    bool forceRefresh = text_[RowTitle].empty();
    if (prevEnabled_ != hiZEnabled) {
        accum_ = GFX::HiZStats{};
        accumFps_ = 0.f; accumSkip_ = 0u; accumTrack_ = 0u; accumCnt_ = 0u;
        prevEnabled_ = hiZEnabled;
        forceRefresh = true;
    }

    accum_.skinnedSubmitted    += stats.skinnedSubmitted;
    accum_.skinnedAfterFrustum += stats.skinnedAfterFrustum;
    accum_.skinnedAfterHiZ     += stats.skinnedAfterHiZ;
    accum_.propSubmitted       += stats.propSubmitted;
    accum_.propAfterFrustum    += stats.propAfterFrustum;
    accum_.propAfterHiZ        += stats.propAfterHiZ;
    accumFps_   += fps;
    accumSkip_  += objSkipped;
    accumTrack_ += objTracked;
    ++accumCnt_;

    if (forceRefresh || accumCnt_ >= static_cast<u32t>(kRefreshFrames)) {
        const u32t n = std::max(1u, accumCnt_);
        const auto avg = GFX::HiZStats{
            .skinnedSubmitted    = accum_.skinnedSubmitted    / n,
            .skinnedAfterFrustum = accum_.skinnedAfterFrustum / n,
            .skinnedAfterHiZ     = accum_.skinnedAfterHiZ     / n,
            .propSubmitted       = accum_.propSubmitted       / n,
            .propAfterFrustum    = accum_.propAfterFrustum    / n,
            .propAfterHiZ        = accum_.propAfterHiZ        / n
        };
        rebuildText(avg, hiZEnabled, accumSkip_ / n, accumTrack_ / n, accumFps_ / n);

        accum_ = GFX::HiZStats{};
        accumFps_ = 0.f; accumSkip_ = 0u; accumTrack_ = 0u; accumCnt_ = 0u;
    }

    // Row set: the Hi-Z stage and the anim/physics skip only exist while Hi-Z runs.
    int drawn[RowCnt] = {};
    int drawnCnt = 0;
    drawn[drawnCnt++] = RowTitle;
    drawn[drawnCnt++] = RowHeader;
    drawn[drawnCnt++] = RowNoCull;
    drawn[drawnCnt++] = RowFrustum;
    if (hiZEnabled) drawn[drawnCnt++] = RowHiZ;
    drawn[drawnCnt++] = RowRatio;
    if (hiZEnabled) drawn[drawnCnt++] = RowSkip;
    drawn[drawnCnt++] = RowFps;

    const float pad      = kPadPx * sizeScale;
    const float rowPitch = kRowPitchPx * sizeScale;
    const float glyphH   = kRowPitchPx * sizeScale;   // clamp for the quad height

    // Widen the plate to the widest row we have measured (0 on the very first frame,
    // which simply falls back to the minimum width and self-corrects next frame).
    float widest = 0.f;
    for (int i = 0; i < drawnCnt; ++i) widest = std::max(widest, measW_[drawn[i]]);
    const float plateW = std::max(kMinWidthPx * sizeScale, widest + pad * 2.f);
    const float plateH = pad * 2.f + rowPitch * static_cast<float>(drawnCnt);

    // Bottom-origin pixels (y grows up), matching uiShapes / the UI shader.
    const float plateLeft = kLeftPx * uiScale;
    const float plateTop  = screenH - kTopPx * uiScale;

    uiShapes::quad(gfx, gfx.solidColorTex(),
                   plateLeft + plateW * 0.5f, plateTop - plateH * 0.5f,
                   plateW, plateH, 0.f, kPlate);

    const float textLeft = plateLeft + pad;
    for (int i = 0; i < drawnCnt; ++i) {
        const int   r  = drawn[i];
        const float cy = plateTop - pad - rowPitch * (static_cast<float>(i) + 0.5f);

        XMFLOAT4 color = kValue;
        if (r == RowTitle)       color = hiZEnabled ? kTitleOn : kTitleOff;
        else if (r == RowHeader) color = kHeader;
        else if (r == RowSkip || r == RowFps || r == RowRatio) color = kFootnote;

        drawRow(gfx, r, textLeft, cy, glyphH, color);

        if (r == RowRatio) {
            const float barX0 = plateLeft + plateW * kBarLeftFrac;
            const float barX1 = plateLeft + plateW - pad;
            const float barW  = std::max(1.f, barX1 - barX0);
            const float barH  = kBarHeightPx * sizeScale;
            uiShapes::quad(gfx, gfx.solidColorTex(), barX0 + barW * 0.5f, cy,
                           barW, barH, 0.f, kBarTrack);
            const float fillW = barW * culledRatio_;
            if (fillW > 0.5f)
                uiShapes::quad(gfx, gfx.solidColorTex(), barX0 + fillW * 0.5f, cy,
                               fillW, barH, 0.f, barFillColor(culledRatio_));
        }
    }
}
