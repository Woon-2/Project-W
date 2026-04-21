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
};

struct RenderContext {
    GFX* gfx = nullptr;
    float screenHeight = 768.f;
};

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
    void layout(const Rect& parentRect);
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
