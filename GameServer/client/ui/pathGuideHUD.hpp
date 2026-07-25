#ifndef __ui_pathGuideHUD_HPP
#define __ui_pathGuideHUD_HPP

// On-screen destination indicator for the client path-guidance route. When the
// look-ahead point is on screen it draws a pulsing beacon; when it is off screen (or
// behind the camera) it clamps a rotating arrow to the screen edge. Distance-to-goal
// is printed (in metres, e.g. "132m") beneath either marker. Resolution-relative,
// driven on the game thread. Purely cosmetic.

#include "../gfxUtil.hpp"   // Texture, XMFLOAT4, mu math types
#include "../font.hpp"      // TextImage (owned distance label)

class GFX;

class PathGuideHUD {
public:
    void setVisible(bool v) { visible_ = v; }
    bool visible() const { return visible_; }

    // Creates the distance-label text target once (call after GFX is ready).
    void init(GFX& gfx);

    // view/proj are the live camera matrices; targetWorld is the guidance look-ahead
    // point (PathGuideSystem::guidanceTargetWorld); distMeters is distance-to-goal.
    // No-op when !active. The distance label is re-rasterised only when the integer
    // metre value changes (mutates cache state -> non-const).
    void render(GFX& gfx, const mu::Mat4x4& view, const mu::Mat4x4& proj,
                mu::Vec3 targetWorld, float distMeters, bool active,
                float screenW, float screenH);

private:
    // Draws the "<n>m" label centred at (cx,cy) in bottom-origin pixels.
    void drawDistanceLabel(GFX& gfx, float cx, float cy, int meters, float uiScale);

    bool      visible_   = true;
    TextImage distText_{};        // owned label target (white glyphs on transparent)
    bool      textReady_ = false; // init() done
    int       lastDist_  = -1;    // cached metre value (re-raster only on change)
    float     textW_     = 0.f;   // measured px of the rasterised label
    float     textH_     = 0.f;

    // Base resolution the pixel sizes are tuned at (matches UIManager base layout).
    static constexpr float kBaseWidth  = 1024.f;
    static constexpr float kBaseHeight = 768.f;
};

#endif  // __ui_pathGuideHUD_HPP
