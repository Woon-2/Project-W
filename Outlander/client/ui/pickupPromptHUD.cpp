#include "pch.hpp"
#include "pickupPromptHUD.hpp"

#include "../gfx.hpp"
#include "../uiPipeline.hpp"
#include "uiShapes.hpp"

#include <algorithm>
#include <cmath>

namespace {

const XMFLOAT4 kPromptText  { 1.00f, 0.94f, 0.72f, 1.00f };
const XMFLOAT4 kPromptPlate { 0.02f, 0.03f, 0.05f, 0.62f };
const XMFLOAT4 kNoticeText  { 1.00f, 0.62f, 0.55f, 1.00f };

}  // namespace

void PickupPromptHUD::init(GFX& gfx) {
    gfx.createTextImageImmediate(512u, 64u, &promptText_);
    gfx.createTextImageImmediate(512u, 64u, &noticeText_);
    ready_ = true;
    promptCache_.clear();
    noticeCache_.clear();
}

void PickupPromptHUD::drawText(GFX& gfx, TextImage& img, std::wstring& cache,
                               float& measuredW, float& measuredH,
                               const std::wstring& text, float cx, float cy,
                               float heightPx, const XMFLOAT4& color) {
    if (!ready_ || text.empty()) return;

    if (text != cache) {
        std::fill(img.pData.begin(), img.pData.end(), static_cast<BYTE>(0));
        int tw = 0, th = 0;
        if (gfx.WriteTextToBitmap(&img, img.width, img.height, img.width * 4u,
                                  &tw, &th, nullptr,
                                  text.c_str(), static_cast<DWORD>(text.size()))) {
            gfx.UpdateTextureWithTextImage(&img, img.width, img.height);
            measuredW = static_cast<float>(tw);
            measuredH = static_cast<float>(th);
            cache     = text;
        }
    }
    if (measuredW <= 0.f || measuredH <= 0.f) return;

    const float qH = heightPx;
    const float qW = qH * (measuredW / measuredH);
    const mu::Mat4x4 world =
          mu::Mat4x4(mu::scale(mu::Vec3{ qW * 0.5f, qH * 0.5f, 1.f }))
        * mu::translate(mu::Vec3{ cx, cy, 0.f });

    gfx.addDrawEvent(UIPipeline::DrawEvent{
        .world       = world,
        .pTex        = &img.texture,
        .pCopySrc    = &img.textureUpload,
        .colorMul    = color,
        .uvScaleBias = XMFLOAT4{ measuredW / static_cast<float>(img.width),
                                 measuredH / static_cast<float>(img.height), 0.f, 0.f },
    });
}

void PickupPromptHUD::render(GFX& gfx, const mu::Mat4x4& view, const mu::Mat4x4& proj,
                             mu::Vec3 worldPos, const std::wstring& text, bool active,
                             float screenW, float screenH)
{
    if (!active || text.empty() || screenW <= 0.f || screenH <= 0.f) return;

    const float uiScale = std::max(0.5f,
        std::min(screenW / kBaseWidth, screenH / kBaseHeight));

    // Same projection as worldToScreen, but bottom-origin pixels to match uiShapes.
    const mu::Vec4 clip = mu::Vec4(worldPos, 1.f) * (view * proj);
    if (clip.w() <= 1e-4f) return;
    const float ndcX = clip.x() / clip.w();
    const float ndcY = clip.y() / clip.w();
    if (ndcX < -1.f || ndcX > 1.f || ndcY < -1.f || ndcY > 1.f) return;

    const float px = (ndcX + 1.f) * 0.5f * screenW;
    const float py = (ndcY + 1.f) * 0.5f * screenH + 34.f * uiScale;   // above the item

    const float glyphH = 18.f * uiScale;
    const float plateW = (promptH_ > 0.f ? glyphH * (promptW_ / promptH_) : 120.f * uiScale)
                       + 18.f * uiScale;
    uiShapes::quad(gfx, gfx.solidColorTex(), px, py,
                   plateW, glyphH + 10.f * uiScale, 0.f, kPromptPlate);
    drawText(gfx, promptText_, promptCache_, promptW_, promptH_,
             text, px, py, glyphH, kPromptText);
}

void PickupPromptHUD::renderNotice(GFX& gfx, const std::wstring& text, float alpha,
                                   float screenW, float screenH)
{
    if (text.empty() || alpha <= 0.f || screenW <= 0.f || screenH <= 0.f) return;

    const float uiScale = std::max(0.5f,
        std::min(screenW / kBaseWidth, screenH / kBaseHeight));
    XMFLOAT4 color = kNoticeText;
    color.w = std::min(1.f, alpha);

    drawText(gfx, noticeText_, noticeCache_, noticeW_, noticeH_,
             text, screenW * 0.5f, screenH * 0.5f - 90.f * uiScale,
             20.f * uiScale, color);
}
