#ifndef __ui_skillDialHUD_HPP
#define __ui_skillDialHUD_HPP

// Bottom-right radial "dial" of the 3 selectable weapon skills (+ a basic-attack
// chip). The selected skill sits at the top of a 120-degree wheel; mouse-wheel
// selection rotates the whole wheel so the selected icon is always centered.
// Each icon shows its charge as a rising, always-undulating liquid fill
// (UIPipeline effect mode) plus an x2/x3 stack badge. Charge is server
// authoritative (driven by S_SkillCharge); this class only renders + animates.

#include "../gfxUtil.hpp"   // Texture, XMFLOAT4

class GFX;

class SkillDialHUD {
public:
    static constexpr int kSlots = 3;

    // slotIcons/cost/cooldown indexed by dial slot (0..2). basicIcon is the
    // left-click weapon icon. digitAtlas drives the x2/x3 stack badge.
    void configure(unsigned weaponOrdinal,
                   const Texture* const slotIcons[kSlots], const Texture* basicIcon,
                   const float slotCost[kSlots], const float slotCooldownMs[kSlots],
                   const Texture* digitAtlas);

    void setVisible(bool v) { visible_ = v; }
    bool visible() const { return visible_; }

    // Server-authoritative per-slot charge (from S_SkillCharge). Returns true if
    // this update crossed 0 -> 1 stack (the slot just became usable), so callers
    // can fire the "ready" cue (sound).
    bool setCharge(int slot, float charge);

    // Wheel selection (wheel down = next, wheel up = prev).
    void selectNext();
    void selectPrev();
    int  selected() const { return selected_; }

    float slotCharge(int slot) const;
    float slotCost(int slot)   const;
    int   slotStacks(int slot) const;
    bool  slotReady(int slot)  const { return slotStacks(slot) >= 1; }

    // "Instant cast + server re-validate" helpers.
    bool canUse(int slot, float nowSec) const;      // >=1 stack and off (predicted) cooldown
    void notePredictedUse(int slot, float nowSec);  // start predicted cooldown on local cast
    void clearPredictedCooldown(int slot);          // on S_SkillUseReject

    void update(float dtSec);                       // animates wheel rotation
    void render(GFX& gfx, float screenW, float screenH) const;

    // Resolution-relative pivot tuning. Position is stored as a fraction of
    // screen width/height (not absolute pixels) so it keeps the same relative
    // placement across resolutions. nudgeMarginPx lets debug input adjust it
    // in familiar pixel units; the conversion to fraction happens here.
    void nudgeMarginPx(float dxPx, float dyPx, float screenW, float screenH);
    float marginXFrac() const { return marginXFrac_; }
    float marginYFrac() const { return marginYFrac_; }

private:
    bool     visible_       = false;
    unsigned weaponOrdinal_ = 0;

    // Wheel pivot offset from the right/bottom screen edge, as a fraction of
    // screen width/height. Defaults reproduce the original 96px margin at the
    // client's default 1024x768 window.
    float marginXFrac_ = 96.f / 1024.f;
    float marginYFrac_ = 96.f / 768.f;

    const Texture* slotIcon_[kSlots] = { nullptr, nullptr, nullptr };
    const Texture* basicIcon_        = nullptr;
    const Texture* digitAtlas_       = nullptr;

    float slotCost_[kSlots]       = { 0.f, 0.f, 0.f };
    float slotCooldownMs_[kSlots] = { 0.f, 0.f, 0.f };
    float charge_[kSlots]         = { 0.f, 0.f, 0.f };
    float cooldownEndSec_[kSlots] = { 0.f, 0.f, 0.f };
    float readyPulse_[kSlots]     = { 0.f, 0.f, 0.f };   // brief scale pop on 0->1 stack

    int   selected_  = 0;
    float rot_       = 0.f;   // current wheel rotation (degrees)
    float rotTarget_ = 0.f;   // target wheel rotation (degrees)
};

#endif  // __ui_skillDialHUD_HPP
