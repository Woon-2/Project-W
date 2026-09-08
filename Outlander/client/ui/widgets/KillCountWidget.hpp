#ifndef __KillCountWidget_HPP
#define __KillCountWidget_HPP

#include "../UIElement.hpp"

namespace UI {

// Presentation values for the Kill Count HUD. v1 spec; tuned in playtest and
// recorded in docs/combatFeedbackUI.md.
struct KillCountTuning {
    float popDuration    = 0.18f;   // kill-increment pop length (seconds)
    float popScale       = 1.25f;   // peak pop scale on the number
    float skullDelay     = 0.016f;  // skull bounce starts ~1 frame late
    float skullBouncePx  = 6.0f;    // skull vertical bounce amplitude (px)
    float streakTimeout  = 4.0f;    // seconds without a kill resets the streak
    int   streakShowMin  = 3;       // show the streak count once it reaches this
    float streakShowTime = 1.2f;    // seconds the streak number stays after a kill
    float milestoneFlash = 0.35f;   // gold flash duration on streak milestones
    float glyphHeight    = 40.0f;   // kill count digit height (px)
    float streakGlyphH   = 22.0f;   // streak digit height (px)
    float skullSizePx    = 40.0f;
    float gapPx          = 8.0f;    // gap between skull and number
};

// Top HUD element: skull icon + accumulated kill count, with a kill pop, a
// short-lived streak number, and a gold flash on streak milestones. Both the
// skull and the digits are drawn through DigitAtlas/UIPipeline for a consistent
// look with the floating damage numbers.
class KillCountWidget : public UIElement {
public:
    void setTextures(const Texture* digitAtlas, const Texture* skull) {
        digitAtlas_ = digitAtlas;
        skull_      = skull;
    }
    void setTuning(const KillCountTuning& t) { tuning_ = t; }

    // Called on the game thread when a monster kill is confirmed.
    void addKill();
    void reset() {
        killCount_ = 0;
        streakCount_ = 0;
        popAge_ = 1.0e9f;
        streakTimer_ = 1.0e9f;
        milestoneFlashAge_ = 1.0e9f;
    }

    int killCount() const { return killCount_; }
    int streak()    const { return streakCount_; }

    void onUpdate(const UpdateContext& ctx) override;
    void onRender(const RenderContext& rc) override;

private:
    const Texture*  digitAtlas_ = nullptr;
    const Texture*  skull_      = nullptr;
    KillCountTuning tuning_{};

    int   killCount_         = 0;
    int   streakCount_       = 0;
    float popAge_            = 1.0e9f;  // large => no active pop
    float streakTimer_       = 1.0e9f;  // time since last kill
    float milestoneFlashAge_ = 1.0e9f;
};

}  // namespace UI

#endif  // __KillCountWidget_HPP
