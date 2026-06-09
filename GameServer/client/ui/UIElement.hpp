#ifndef __UI_ELEMENT_HPP
#define __UI_ELEMENT_HPP

#include "UITypes.hpp"

class GFX;
struct FontHandle;
struct Texture;

namespace UI {

struct UpdateContext {
    float deltaTimeSec = 0.f;
    GFX* gfx = nullptr;
    FontHandle* defaultFont = nullptr;
    float screenWidth = 1024.f;
    float screenHeight = 768.f;
    float uiScale = 1.f;
};

struct RenderContext {
    GFX* gfx = nullptr;
    float screenHeight = 768.f;
    float uiScale = 1.f;
};

// Emits 9 textured quads (a 9-slice) covering `rect`. Corners keep a fixed
// screen size (cornerX/cornerY, in pixels) while edges/center stretch.
// uvBorderX/Y are the slice insets as a fraction (0..0.5) of the source texture.
void emitNineSlice(const RenderContext& rc, const Rect& rect, const Texture* tex,
                   float uvBorderX, float uvBorderY,
                   float cornerX, float cornerY,
                   const XMFLOAT4& colorMul);

class UIElement {
public:
    virtual ~UIElement() = default;

    // --- Properties ---
    std::string name;
    Anchor   anchor       = Anchors::TopLeft;
    Pivot    pivot         = Pivots::TopLeft;
    DimValue offsetX;
    DimValue offsetY;
    DimValue width;
    DimValue height;
    Color    colorTint     = { 1.f, 1.f, 1.f, 1.f };
    int      zOrder        = 0;
    bool     visible       = true;
    bool     interactive   = false;

    // --- Hierarchy ---
    UIElement* parent() const { return parent_; }
    const std::vector<std::unique_ptr<UIElement>>& children() const { return children_; }

    UIElement* addChild(std::unique_ptr<UIElement> child);
    void removeChild(UIElement* child);
    UIElement* findChild(std::string_view name) const;

    // --- Layout ---
    void layout(const Rect& parentRect, float pixelScale = 1.f);
    const Rect& resolvedRect() const { return resolvedRect_; }

    // --- Update / Render ---
    virtual void onUpdate(const UpdateContext& ctx) {}
    virtual void onRender(const RenderContext& rc) {}

    void updateTree(const UpdateContext& ctx);
    void renderTree(const RenderContext& rc);

    // --- Input callbacks ---
    virtual void onMouseEnter() {}
    virtual void onMouseLeave() {}
    virtual void onMouseDown(MouseButton btn, float localX, float localY) {}
    virtual void onMouseUp(MouseButton btn, float localX, float localY) {}
    virtual void onMouseMove(float localX, float localY) {}
    virtual void onKeyDown(int vkCode) {}
    virtual void onKeyUp(int vkCode) {}
    // Character input (WM_CHAR), routed to the focused element by UIManager.
    virtual void onChar(wchar_t ch) {}
    // Focus gained/lost. UIManager calls these when the focused element changes.
    virtual void onFocus() {}
    virtual void onBlur() {}

protected:
    // Build world matrix from resolvedRect_ for UIPipeline.
    // screenHeight is needed for Y-axis flip (layout is top-down, shader Y=0 is bottom).
    mu::Mat4x4 buildWorldMatrix(float screenHeight) const;

    Rect resolvedRect_{};

private:
    UIElement* parent_ = nullptr;
    std::vector<std::unique_ptr<UIElement>> children_;
};

}   // namespace UI

#endif  // __UI_ELEMENT_HPP
