#ifndef __UI_MANAGER_HPP
#define __UI_MANAGER_HPP

#include "UIElement.hpp"
#include "../font.hpp"

class GFX;
struct FontHandle;

namespace UI {

class UIManager {
public:
    UIManager() = default;

    void setScreenSize(float w, float h);
    float screenWidth() const { return screenWidth_; }
    float screenHeight() const { return screenHeight_; }
    float layoutWidth() const { return kBaseWidth; }
    float layoutHeight() const { return kBaseHeight; }
    float uiScale() const { return uiScale_; }
    const Rect& safeAreaRect() const { return safeAreaRect_; }
    float screenToLayoutX(float x) const;
    float screenToLayoutY(float y) const;
    float screenLeftInsetToLayoutX(float baseInset) const;
    float screenTopInsetToLayoutY(float baseInset) const;

    // Root element. All UI elements are children of root.
    UIElement* root() { return &root_; }

    // Per-frame calls (in order).
    void layout();
    void update(float deltaTimeSec, GFX& gfx, FontHandle* defaultFont);
    void render(GFX& gfx);

    // Input routing. Returns true if the UI consumed the message.
    bool onWndMsg(UINT msg, WPARAM wParam, LPARAM lParam);

    // True when an interactive element is visible (game should release cursor).
    bool needsCursor() const;

    // Clears cached hover/press/focus pointers. Call before destroying/rebuilding
    // the widget tree (e.g. on a resolution change) so these raw pointers cannot
    // dangle into freed widgets on the next input event.
    void resetInteractionState();

    // Debug overlay: draws colored bounding boxes for all visible elements.
    // Call requestDebugResources(gfx) once after loadAssets() before using.
    void requestDebugResources(GFX& gfx);
    void toggleDebugMode() { debugMode_ = !debugMode_; }
    bool debugMode() const { return debugMode_; }

private:
    UIElement* hitTest(float x, float y);
    void collectInteractive(UIElement* elem, std::vector<UIElement*>& out);
    void renderDebugOverlay(GFX& gfx);

    static constexpr float kBaseWidth = 1024.f;
    static constexpr float kBaseHeight = 768.f;

    UIElement root_;
    float screenWidth_  = 1024.f;
    float screenHeight_ = 768.f;
    float uiScale_ = 1.f;
    Rect  safeAreaRect_{ 0.f, 0.f, 1024.f, 768.f };

    UIElement* hoveredElement_ = nullptr;
    UIElement* pressedElement_ = nullptr;
    UIElement* focusedElement_ = nullptr;

    float cursorX_ = 0.f;
    float cursorY_ = 0.f;

    bool       debugMode_               = false;
    TextImage  debugWhiteTex_;
    bool       debugWhiteTexReady_      = false;
    bool       debugWhiteTexNeedsCopy_  = false;
};

}   // namespace UI

#endif  // __UI_MANAGER_HPP
