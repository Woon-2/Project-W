#ifndef __ui_hiZStatsHUD_HPP
#define __ui_hiZStatsHUD_HPP

// Left-middle culling statistics overlay: how many draw instances survive each
// culling stage (no culling -> view frustum -> Hi-Z occlusion), split into skinned
// characters and static/scatter props.
//
// Modelled on PathGuideHUD / PickupPromptHUD: an immediate-mode HUD that lives
// outside the UIManager tree, owns one offscreen text target per row and submits
// UIPipeline draw events directly. Rows are re-rasterised only when their string
// changes, and the strings themselves refresh a few times per second so the numbers
// stay readable and D2D work stays off the per-frame path.
//
// Rows are composed at render time, so a stage that has no meaning right now is
// simply not drawn (the panel shrinks) instead of being blanked out. With the
// overlay hidden nothing at all is submitted.

#include "../gfx.hpp"     // GFX::HiZStats
#include "../font.hpp"    // TextImage, FontHandle

class HiZStatsHUD {
public:
    // Creates the row text targets. Idempotent: Online::Game::setupStage runs again
    // on every re-entry into a match, and re-creating live GPU text targets there
    // would free memory that in-flight frames may still reference.
    void init(GFX& gfx);

    void setVisible(bool v) { visible_ = v; }
    bool visible() const { return visible_; }

    // Draws the panel. No-op unless init() ran and the overlay is visible.
    // hiZEnabled == false omits the Hi-Z stage row and the anim/physics skip row
    // entirely (those numbers do not exist while the Hi-Z pass is off).
    // objSkipped/objTracked are object-level counts from the game's Hi-Z feedback.
    void render(GFX& gfx, const GFX::HiZStats& stats, bool hiZEnabled,
                u32t objSkipped, u32t objTracked, float fps,
                float screenW, float screenH);

private:
    // Fixed row slots. Keeping the index stable per line keeps each row's raster
    // cache warm even when the visible row set changes.
    enum Row : int {
        RowTitle = 0, RowHeader, RowNoCull, RowFrustum, RowHiZ,
        RowRatio, RowSkip, RowFps, RowCnt
    };

    void ensureFont(GFX& gfx, float uiScale);
    void rebuildText(const GFX::HiZStats& avg, bool hiZEnabled,
                     u32t objSkipped, u32t objTracked, float fps);
    // Draws row `r` with its left edge at leftPx, vertically centred on cy
    // (bottom-origin pixels, matching uiShapes).
    void drawRow(GFX& gfx, int r, float leftPx, float cy,
                 float glyphH, const XMFLOAT4& color);

    TextImage    rows_[RowCnt]{};
    std::wstring text_[RowCnt]{};    // formatted at refresh time
    std::wstring cache_[RowCnt]{};   // last rasterised string
    float        measW_[RowCnt]{};
    float        measH_[RowCnt]{};
    // Upload->texture copy is only needed on the frame the row was re-rasterised.
    // Handing UIPipeline a pCopySrc every frame makes it emit two resource barriers
    // and a full texture copy per row, mid-pass — that alone halved the frame rate.
    bool         needsCopy_[RowCnt]{};

    FontHandle font_{};              // monospace: keeps the three columns aligned
    float      fontScale_ = -1.f;    // uiScale the font was created at

    // Averaging window. Raw per-frame counts jitter too much to read.
    GFX::HiZStats accum_{};
    float         accumFps_   = 0.f;
    u32t          accumSkip_  = 0u;
    u32t          accumTrack_ = 0u;
    u32t          accumCnt_   = 0u;
    bool          prevEnabled_ = true;
    float         culledRatio_ = 0.f;   // drives the bar, refreshed with the text

    bool visible_ = true;
    bool ready_   = false;

    static constexpr int   kRefreshFrames = 10;    // ~6 Hz at 60 fps
    static constexpr float kBaseWidth  = 1024.f;
    static constexpr float kBaseHeight = 768.f;
};

#endif  // __ui_hiZStatsHUD_HPP
