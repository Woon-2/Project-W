#ifndef __UI_SCROLLVIEW_HPP
#define __UI_SCROLLVIEW_HPP

#include "../UIElement.hpp"

namespace UI {

// A vertically scrollable viewport. Widgets added under content() are clipped to
// this element's rect (GPU scissor via clipsChildren) and scrolled with the mouse
// wheel. Metrics are in layout (virtual) units, matching DimValue::px.
class ScrollView : public UIElement {
public:
    ScrollView();

    // Content host: add scrollable widgets as children of this element.
    UIElement* content() { return content_; }

    // Total content height and the visible viewport height (layout px). The owner
    // sets these after building the content so the wheel can clamp the scroll.
    float contentHeight  = 0.f;
    float viewportHeight = 0.f;
    float scrollStep     = 48.f;   // layout px per wheel notch

    bool onMouseWheel(float delta) override;

private:
    UIElement* content_ = nullptr;
    float      scrollY_ = 0.f;
};

}   // namespace UI

#endif  // __UI_SCROLLVIEW_HPP
