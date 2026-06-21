#ifndef __ui_minimapHUD_HPP
#define __ui_minimapHUD_HPP

// Top-left minimap: a cached top-down terrain texture (re-baked by GFX only when
// the streamed chunk set changes) plus a thin frame and per-entity colored square
// icons recomputed every frame from a simple North-up world-XZ -> local offset
// projection (no 3D rendering involved for the icons).

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

    // playerWorldPos centers the map; worldRadius is the world-space half-extent
    // shown (should match the radius the background cache was baked with).
    void render(GFX& gfx, mu::Vec3 playerWorldPos, float worldRadius,
                const std::vector<MinimapEntityIcon>& entities,
                float screenW, float screenH) const;

private:
    bool  visible_     = true;
    float marginXFrac_ = 24.f / 1024.f;
    float marginYFrac_ = 24.f / 768.f;
    float sizePx_       = 200.f;
    float borderPx_     = 4.f;
};

#endif  // __ui_minimapHUD_HPP
