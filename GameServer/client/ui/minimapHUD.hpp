#ifndef __ui_minimapHUD_HPP
#define __ui_minimapHUD_HPP

// Top-left minimap. The terrain background is a world-fixed cached texture (re-baked
// by GFX only when the streamed chunk set changes) covering an N x N chunk region; the
// HUD scrolls it every frame by sampling a UV sub-rect derived from the player's world
// position and the current zoom, so the player stays centered while terrain slides.
// Entity icons are colored squares projected from world XZ relative to the player (no
// 3D rendering). North-up. Size/position are resolution-relative (mirrors SkillDialHUD).

#include "../mathUtil.hpp"

#include <vector>

class GFX;

struct MinimapEntityIcon {
    enum class Kind { Self, Party, Monster, Boss };
    mu::Vec3 worldPos{};
    Kind     kind = Kind::Monster;
};

class MinimapHUD {
public:
    void setVisible(bool v) { visible_ = v; }
    bool visible() const { return visible_; }

    // Shift+wheel zoom. zoomIn shows less area (more detail); zoomOut shows more.
    void zoomIn();
    void zoomOut();

    // bakedCenter/bakedCoverage describe the world-space square the cache texture covers
    // (from TerrainChunkManager::minimapBakeRegion). bakedCoverage <= 0 => background
    // not ready yet (frame + icons still draw).
    void render(GFX& gfx, mu::Vec3 playerWorldPos,
                mu::Vec3 bakedCenter, float bakedCoverage,
                const std::vector<MinimapEntityIcon>& entities,
                float screenW, float screenH) const;

private:
    bool  visible_ = true;

    // Position as a fraction of screen size (top-left origin); size in base-resolution
    // pixels, scaled by uiScale at render time. Matches SkillDialHUD's approach.
    float marginXFrac_  = 24.f / 1024.f;
    float marginYFrac_  = 24.f / 768.f;
    float baseSizePx_   = 200.f;
    float baseBorderPx_ = 4.f;

    // Zoom: effective view radius = baseViewRadius_ / zoom_ (clamped). zoom_ > 1 = zoomed in.
    float baseViewRadius_ = 60.f;   // matches GFX::kMinimapWorldRadius
    float zoom_           = 1.f;

    // Base resolution the size constants are tuned at (matches UIManager base layout).
    static constexpr float kBaseWidth  = 1024.f;
    static constexpr float kBaseHeight = 768.f;
    static constexpr float kZoomMin    = 0.5f;   // zoomed out (larger view radius)
    static constexpr float kZoomMax    = 4.0f;   // zoomed in  (smaller view radius)
    static constexpr float kZoomStep   = 1.25f;
};

#endif  // __ui_minimapHUD_HPP
