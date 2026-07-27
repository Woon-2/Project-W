#ifndef __ui_pickupPromptHUD_HPP
#define __ui_pickupPromptHUD_HPP

// World-anchored interaction prompt for the aimed world item drop ("[F] pick up
// - <item name>"), plus a short-lived centre-screen notice for rejected pickups.
// Modelled on PathGuideHUD: an owned offscreen text target that is re-rasterised
// only when the string changes, submitted through UIPipeline.
//
// The prompt text itself is composed by the caller and handed in as a wide string,
// so no localised literal lives in this file.

#include "../gfxUtil.hpp"
#include "../font.hpp"

class GFX;

class PickupPromptHUD {
public:
    void init(GFX& gfx);

    // Draws the prompt anchored above worldPos. No-op when !active or when the
    // anchor is off screen. Mutates the raster cache -> non-const.
    void render(GFX& gfx, const mu::Mat4x4& view, const mu::Mat4x4& proj,
                mu::Vec3 worldPos, const std::wstring& text, bool active,
                float screenW, float screenH);

    // Centre-screen failure notice (inventory full, already taken, ...).
    void renderNotice(GFX& gfx, const std::wstring& text, float alpha,
                      float screenW, float screenH);

private:
    // Rasterises `text` into `img` when it differs from `cache`, then draws the
    // measured sub-rect centred at (cx, cy) in bottom-origin pixels.
    void drawText(GFX& gfx, TextImage& img, std::wstring& cache,
                  float& measuredW, float& measuredH,
                  const std::wstring& text, float cx, float cy,
                  float heightPx, const XMFLOAT4& color);

    TextImage    promptText_{};
    TextImage    noticeText_{};
    std::wstring promptCache_{};
    std::wstring noticeCache_{};
    float        promptW_ = 0.f, promptH_ = 0.f;
    float        noticeW_ = 0.f, noticeH_ = 0.f;
    bool         ready_   = false;

    static constexpr float kBaseWidth  = 1024.f;
    static constexpr float kBaseHeight = 768.f;
};

#endif  // __ui_pickupPromptHUD_HPP
