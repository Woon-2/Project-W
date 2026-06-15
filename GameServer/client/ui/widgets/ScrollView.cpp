#include "pch.hpp"
#include "ScrollView.hpp"

#include <algorithm>

namespace UI {

ScrollView::ScrollView() {
    interactive   = true;   // be the hovered element over gaps so the wheel routes here
    clipsChildren = true;   // clip content to the viewport rect (GPU scissor)

    auto c = std::make_unique<UIElement>();
    c->name    = "scrollContent";
    c->anchor  = Anchors::TopLeft;
    c->pivot   = Pivots::TopLeft;
    c->offsetX = DimValue::px(0.f);
    c->offsetY = DimValue::px(0.f);
    c->width   = DimValue::pct(100.f);
    c->height  = DimValue::pct(100.f);
    content_ = addChild(std::move(c));
}

bool ScrollView::onMouseWheel(float delta) {
    const float maxScroll = std::max(0.f, contentHeight - viewportHeight);
    if (maxScroll > 0.f) {
        // Wheel forward (delta > 0) reveals the top -> decrease scroll offset.
        scrollY_ = std::clamp(scrollY_ - delta * scrollStep, 0.f, maxScroll);
        content_->offsetY = DimValue::px(-scrollY_);
    }
    return true;   // consume so the wheel never falls through to the game camera
}

}   // namespace UI
