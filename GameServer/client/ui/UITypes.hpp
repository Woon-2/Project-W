#ifndef __UI_TYPES_HPP
#define __UI_TYPES_HPP

// No Korean comments in new files (encoding issue per CLAUDE.md)

namespace UI {

// Normalized anchor point on parent rectangle.
// (0,0) = top-left, (1,1) = bottom-right.
struct Anchor {
    float x = 0.f;
    float y = 0.f;
};

// Normalized pivot point on the element itself.
// Determines which point is placed at the anchored position.
struct Pivot {
    float x = 0.f;
    float y = 0.f;
};

// Value expressed as pixels or percentage of parent dimension.
struct DimValue {
    float value = 0.f;
    bool isPercent = false;

    float resolve(float parentDim) const {
        return isPercent ? (value * 0.01f * parentDim) : value;
    }

    static DimValue px(float v) { return { v, false }; }
    static DimValue pct(float v) { return { v, true }; }
};

// Resolved screen-space rectangle (absolute pixel coordinates).
// (0,0) = top-left of screen, Y increases downward.
struct Rect {
    float x = 0.f;
    float y = 0.f;
    float width = 0.f;
    float height = 0.f;

    bool contains(float px, float py) const {
        return px >= x && px <= x + width && py >= y && py <= y + height;
    }
};

struct Color {
    float r = 1.f, g = 1.f, b = 1.f, a = 1.f;
};

enum class MouseButton { Left, Right, Middle };

namespace Anchors {
    inline constexpr Anchor TopLeft      = { 0.f, 0.f };
    inline constexpr Anchor TopCenter    = { 0.5f, 0.f };
    inline constexpr Anchor TopRight     = { 1.f, 0.f };
    inline constexpr Anchor CenterLeft   = { 0.f, 0.5f };
    inline constexpr Anchor Center       = { 0.5f, 0.5f };
    inline constexpr Anchor CenterRight  = { 1.f, 0.5f };
    inline constexpr Anchor BottomLeft   = { 0.f, 1.f };
    inline constexpr Anchor BottomCenter = { 0.5f, 1.f };
    inline constexpr Anchor BottomRight  = { 1.f, 1.f };
}

namespace Pivots {
    inline constexpr Pivot TopLeft      = { 0.f, 0.f };
    inline constexpr Pivot TopCenter    = { 0.5f, 0.f };
    inline constexpr Pivot TopRight     = { 1.f, 0.f };
    inline constexpr Pivot CenterLeft   = { 0.f, 0.5f };
    inline constexpr Pivot Center       = { 0.5f, 0.5f };
    inline constexpr Pivot CenterRight  = { 1.f, 0.5f };
    inline constexpr Pivot BottomLeft   = { 0.f, 1.f };
    inline constexpr Pivot BottomCenter = { 0.5f, 1.f };
    inline constexpr Pivot BottomRight  = { 1.f, 1.f };
}

}   // namespace UI

#endif  // __UI_TYPES_HPP
