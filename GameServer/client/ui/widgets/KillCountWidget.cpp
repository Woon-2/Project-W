#include "pch.hpp"
#include "KillCountWidget.hpp"
#include "../digitAtlas.hpp"
#include "../../gfx.hpp"
#include <algorithm>
#include <cmath>

namespace UI {

namespace {
    bool  isMilestone(int s) { return s == 10 || s == 25 || s == 50 || s == 100; }
    float lerp(float a, float b, float t) { return a + (b - a) * t; }
    constexpr float kPi = 3.14159265f;
}

void KillCountWidget::addKill() {
    ++killCount_;
    ++streakCount_;
    popAge_      = 0.f;
    streakTimer_ = 0.f;
    if (isMilestone(streakCount_)) milestoneFlashAge_ = 0.f;
}

void KillCountWidget::onUpdate(const UpdateContext& ctx) {
    const float dt = ctx.deltaTimeSec;
    popAge_            += dt;
    milestoneFlashAge_ += dt;
    if (streakCount_ > 0) {
        streakTimer_ += dt;
        if (streakTimer_ >= tuning_.streakTimeout) streakCount_ = 0;
    }
}

void KillCountWidget::onRender(const RenderContext& rc) {
    if (!visible || !digitAtlas_ || !rc.gfx) return;

    GFX&        gfx     = *rc.gfx;
    const float screenH = rc.screenHeight;
    const Rect& r       = resolvedRect_;
    const float cy      = r.y + r.height * 0.5f;

    // Kill-increment pop: 1 -> popScale -> 1 over popDuration.
    float pop = 1.f;
    if (popAge_ < tuning_.popDuration) {
        const float p = popAge_ / tuning_.popDuration;
        pop = (p < 0.5f) ? lerp(1.f, tuning_.popScale, p / 0.5f)
                         : lerp(tuning_.popScale, 1.f, (p - 0.5f) / 0.5f);
    }

    // Gold flash on streak milestones.
    const bool     flashing = milestoneFlashAge_ < tuning_.milestoneFlash;
    const XMFLOAT4 numColor = flashing ? XMFLOAT4{ 1.0f, 0.84f, 0.0f, 1.0f }
                                       : XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };

    // Skull icon (left), with a slightly delayed bounce on each kill.
    if (skull_) {
        float bounce = 0.f;
        const float bAge = popAge_ - tuning_.skullDelay;
        if (bAge >= 0.f && bAge < tuning_.popDuration) {
            bounce = tuning_.skullBouncePx * rc.uiScale * std::sin((bAge / tuning_.popDuration) * kPi);
        }
        const float sz  = tuning_.skullSizePx * rc.uiScale;
        const float scx = r.x + sz * 0.5f;
        const float scy = cy - bounce;
        const mu::Vec3 scl{ sz * 0.5f, sz * 0.5f, 1.f };
        const mu::Vec3 pos{ scx, screenH - scy, 0.f };
        gfx.addDrawEvent(UIPipeline::DrawEvent{
            .world    = mu::Mat4x4(mu::scale(scl)) * mu::translate(pos),
            .pTex     = skull_,
            .colorMul = numColor
        });
    }

    // Accumulated kill count (left-aligned, right of the skull).
    const float numH = tuning_.glyphHeight * pop;
    const float numX = r.x + (tuning_.skullSizePx + tuning_.gapPx) * rc.uiScale;
    DigitAtlas::emitNumber(gfx, digitAtlas_, numX, cy, numH * rc.uiScale, screenH,
                           killCount_, numColor, DigitAtlas::Align::Left);

    // Streak count: small and gold, shown briefly after each kill.
    if (streakCount_ >= tuning_.streakShowMin && streakTimer_ < tuning_.streakShowTime) {
        const float a = std::clamp((1.f - streakTimer_ / tuning_.streakShowTime) * 1.5f, 0.f, 1.f);
        const XMFLOAT4 streakColor{ 1.0f, 0.75f, 0.1f, a };
        const float sy = cy + (tuning_.glyphHeight * 0.6f + tuning_.streakGlyphH * 0.6f) * rc.uiScale;
        DigitAtlas::emitNumber(gfx, digitAtlas_, numX, sy, tuning_.streakGlyphH * rc.uiScale, screenH,
                               streakCount_, streakColor, DigitAtlas::Align::Left);
    }
}

}  // namespace UI
