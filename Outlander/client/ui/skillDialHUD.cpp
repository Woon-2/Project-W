#include "pch.hpp"
#include "skillDialHUD.hpp"
#include "../gfx.hpp"
#include "../uiPipeline.hpp"
#include "digitAtlas.hpp"

#include <cmath>
#include <algorithm>

namespace {
    constexpr float kPi       = 3.14159265358979323846f;
    constexpr float kTopDeg   = 90.f;    // selected slot sits at the top of the wheel
    constexpr float kStepDeg  = 120.f;   // 3 slots evenly spaced -> symmetric, no teleport
    constexpr float kRadius   = 62.f;    // slot ring radius (icons sit on the donut band)
    constexpr float kSelSize  = 60.f;
    constexpr float kSideSize = 44.f;

    constexpr float kRingPad  = 30.f;    // donut outer edge extends this far past kRadius
    constexpr float kRingHole = 0.42f;   // donut inner radius fraction (see ui.hlsl mode 3)

    // Reference resolution all the size constants above are tuned at (matches
    // UI::UIManager's base layout size). Render scales them by screen size relative
    // to this so the dial keeps its on-screen proportions across resolutions.
    constexpr float kBaseWidth  = 1024.f;
    constexpr float kBaseHeight = 768.f;

    inline mu::Mat4x4 quadWorld(float cx, float cy, float size) {
        const float h = size * 0.5f;
        return mu::Mat4x4(mu::scale(mu::Vec3{ h, h, 1.f }))
             * mu::translate(mu::Vec3{ cx, cy, 0.f });
    }
}

void SkillDialHUD::configure(unsigned weaponOrdinal,
                             const Texture* const slotIcons[kSlots], const Texture* basicIcon,
                             const float slotCost[kSlots], const float slotCooldownMs[kSlots],
                             const Texture* digitAtlas) {
    weaponOrdinal_ = weaponOrdinal;
    basicIcon_  = basicIcon;
    digitAtlas_ = digitAtlas;
    for (int i = 0; i < kSlots; ++i) {
        slotIcon_[i]       = slotIcons[i];
        slotCost_[i]       = slotCost[i];
        slotCooldownMs_[i] = slotCooldownMs[i];
        charge_[i]         = 0.f;
        targetCharge_[i]   = 0.f;
        cooldownEndSec_[i] = 0.f;
    }
    selected_  = 0;
    rot_       = 0.f;
    rotTarget_ = 0.f;
}

bool SkillDialHUD::setCharge(int slot, float charge) {
    if (slot < 0 || slot >= kSlots) return false;
    const int prevStacks = slotStacks(slot);
    charge_[slot] = charge;
    const int newStacks = slotStacks(slot);
    const bool becameReady = (prevStacks < 1 && newStacks >= 1);
    if (becameReady) readyPulse_[slot] = 0.45f;   // trigger the scale pop
    return becameReady;
}

void SkillDialHUD::setChargeTarget(int slot, float target) {
    if (slot < 0 || slot >= kSlots) return;
    targetCharge_[slot] = target;
}

bool SkillDialHUD::addDisplayCharge(int slot, float amount) {
    if (slot < 0 || slot >= kSlots) return false;
    const float capped = std::min(charge_[slot] + amount, targetCharge_[slot]);
    return setCharge(slot, capped);   // reuse the 0 -> 1 ready-pulse logic
}

bool SkillDialHUD::syncDisplayToTarget(int slot) {
    if (slot < 0 || slot >= kSlots) return false;
    return setCharge(slot, targetCharge_[slot]);
}

void SkillDialHUD::selectNext() { selected_ = (selected_ + 1) % kSlots;         rotTarget_ -= kStepDeg; }
void SkillDialHUD::selectPrev() { selected_ = (selected_ + kSlots - 1) % kSlots; rotTarget_ += kStepDeg; }

float SkillDialHUD::slotCharge(int slot) const { return (slot >= 0 && slot < kSlots) ? charge_[slot]   : 0.f; }
float SkillDialHUD::slotCost(int slot)   const { return (slot >= 0 && slot < kSlots) ? slotCost_[slot] : 0.f; }

int SkillDialHUD::slotStacks(int slot) const {
    if (slot < 0 || slot >= kSlots) return 0;
    const float c = slotCost_[slot];
    return (c > 0.f) ? static_cast<int>(charge_[slot] / c) : 0;
}

bool SkillDialHUD::canUse(int slot, float nowSec) const {
    return slotStacks(slot) >= 1 && nowSec >= cooldownEndSec_[slot];
}

void SkillDialHUD::notePredictedUse(int slot, float nowSec) {
    if (slot >= 0 && slot < kSlots)
        cooldownEndSec_[slot] = nowSec + slotCooldownMs_[slot] / 1000.f;
}

void SkillDialHUD::clearPredictedCooldown(int slot) {
    if (slot >= 0 && slot < kSlots) cooldownEndSec_[slot] = 0.f;
}

void SkillDialHUD::update(float dtSec) {
    const float k = std::min(1.f, dtSec * 12.f);   // critically-damped feel
    rot_ += (rotTarget_ - rot_) * k;
    for (int i = 0; i < kSlots; ++i)
        readyPulse_[i] = std::max(0.f, readyPulse_[i] - dtSec);
}

void SkillDialHUD::nudgeMarginPx(float dxPx, float dyPx, float screenW, float screenH) {
    if (screenW > 0.f) marginXFrac_ = std::clamp(marginXFrac_ + dxPx / screenW, 0.f, 0.45f);
    if (screenH > 0.f) marginYFrac_ = std::clamp(marginYFrac_ + dyPx / screenH, 0.f, 0.45f);
}

void SkillDialHUD::render(GFX& gfx, float screenW, float screenH) const {
    if (!visible_) return;

    const float pivotX = screenW * (1.f - marginXFrac_);
    const float pivotY = screenH * marginYFrac_;   // bottom-origin pixel space (matches UI shader)

    const float scaleX = (kBaseWidth  > 0.f) ? screenW / kBaseWidth  : 1.f;
    const float scaleY = (kBaseHeight > 0.f) ? screenH / kBaseHeight : 1.f;
    const float uiScale  = std::max(0.01f, std::min(scaleX, scaleY));
    const float radius   = kRadius   * uiScale;
    const float selSize  = kSelSize  * uiScale;
    const float sideSize = kSideSize * uiScale;
    const float ringPad  = kRingPad  * uiScale;

    // Semi-transparent gray donut the icons sit on. Procedural (ui.hlsl mode 3):
    // no texture is sampled, so any valid texture pointer satisfies the pipeline.
    const Texture* anyTex = digitAtlas_ ? digitAtlas_ : basicIcon_;
    for (int i = 0; !anyTex && i < kSlots; ++i) anyTex = slotIcon_[i];
    if (anyTex) {
        const float ringSize = (radius + ringPad) * 2.f;
        gfx.addDrawEvent(UIPipeline::DrawEvent{
            .world      = quadWorld(pivotX, pivotY, ringSize),
            .pTex       = anyTex,
            .colorMul   = XMFLOAT4{ 0.13f, 0.14f, 0.17f, 0.46f },   // translucent gray
            .fillAmount = kRingHole,
            .effectMode = 3.f,
        });
    }

    struct IconDraw { float cx, cy, size, fill; int mode; bool selected; int slot; };
    IconDraw draws[kSlots];

    for (int i = 0; i < kSlots; ++i) {
        const float a     = (kTopDeg + i * kStepDeg + rot_) * (kPi / 180.f);
        const bool  sel   = (i == selected_);
        const float pulse = readyPulse_[i] / 0.45f;                 // 0..1, eased by linear decay
        const float size  = (sel ? selSize : sideSize) * (1.f + pulse * 0.22f);
        const float cx   = pivotX + radius * std::cos(a);
        const float cy   = pivotY + radius * std::sin(a);

        const int   stacks = slotStacks(i);
        const float cost   = slotCost_[i];
        const float nf     = (cost > 0.f) ? (charge_[i] - stacks * cost) / cost : 0.f;
        draws[i] = { cx, cy, size, nf, (stacks >= 1 ? 2 : 1), sel, i };
    }

    // Icons: non-selected first, selected last so the selected one draws on top.
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < kSlots; ++i) {
            const IconDraw& d = draws[i];
            if (d.selected != (pass == 1) || !slotIcon_[d.slot]) continue;
            const XMFLOAT4 tint = d.selected ? XMFLOAT4{ 1.f, 1.f, 1.f, 1.f }
                                             : XMFLOAT4{ 0.72f, 0.74f, 0.80f, 0.90f };
            gfx.addDrawEvent(UIPipeline::DrawEvent{
                .world      = quadWorld(d.cx, d.cy, d.size),
                .pTex       = slotIcon_[d.slot],
                .colorMul   = tint,
                .fillAmount = d.fill,
                .effectMode = static_cast<float>(d.mode),
            });
        }
    }

    // Stack badges (x2, x3, ...): only once 2+ casts are banked.
    if (digitAtlas_) {
        for (int i = 0; i < kSlots; ++i) {
            const int stacks = slotStacks(i);
            if (stacks < 2) continue;
            const IconDraw& d = draws[i];
            const float topLeftY = screenH - d.cy;   // bottom-origin -> top-left origin
            DigitAtlas::emitNumber(gfx, digitAtlas_,
                d.cx + d.size * 0.30f, topLeftY - d.size * 0.30f,
                d.size * 0.34f, screenH, stacks,
                XMFLOAT4{ 1.f, 0.82f, 0.30f, 1.f }, DigitAtlas::Align::Center);
        }
    }

    // Basic-attack chip (weapon icon, left-click) nested in the donut hole at the
    // dial center.
    if (basicIcon_) {
        gfx.addDrawEvent(UIPipeline::DrawEvent{
            .world    = quadWorld(pivotX, pivotY, sideSize * 0.95f),
            .pTex     = basicIcon_,
            .colorMul = XMFLOAT4{ 0.95f, 0.95f, 0.95f, 0.97f },
        });
    }
}
