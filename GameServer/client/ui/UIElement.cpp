#include "pch.hpp"
#include "UIElement.hpp"

namespace UI {

UIElement* UIElement::addChild(std::unique_ptr<UIElement> child) {
    child->parent_ = this;
    children_.push_back(std::move(child));
    return children_.back().get();
}

void UIElement::removeChild(UIElement* child) {
    std::erase_if(children_, [child](const std::unique_ptr<UIElement>& c) {
        return c.get() == child;
    });
}

UIElement* UIElement::findChild(std::string_view childName) const {
    for (auto& c : children_) {
        if (c->name == childName) return c.get();
    }
    for (auto& c : children_) {
        auto* found = c->findChild(childName);
        if (found) return found;
    }
    return nullptr;
}

void UIElement::layout(const Rect& parentRect) {
    float refX = parentRect.x + anchor.x * parentRect.width;
    float refY = parentRect.y + anchor.y * parentRect.height;

    float w = width.resolve(parentRect.width);
    float h = height.resolve(parentRect.height);

    float ox = offsetX.resolve(parentRect.width);
    float oy = offsetY.resolve(parentRect.height);

    resolvedRect_.x = refX + ox - pivot.x * w;
    resolvedRect_.y = refY + oy - pivot.y * h;
    resolvedRect_.width = w;
    resolvedRect_.height = h;

    for (auto& child : children_) {
        child->layout(resolvedRect_);
    }
}

void UIElement::updateTree(const UpdateContext& ctx) {
    if (!visible) return;

    onUpdate(ctx);
    for (auto& child : children_) {
        child->updateTree(ctx);
    }
}

void UIElement::renderTree(const RenderContext& rc) {
    if (!visible) return;

    onRender(rc);

    // Sort children by zOrder for correct draw order.
    // UIPipeline draws in submission order with no depth test.
    std::vector<UIElement*> sorted;
    sorted.reserve(children_.size());
    for (auto& c : children_) {
        if (c->visible) sorted.push_back(c.get());
    }
    std::ranges::sort(sorted, {}, &UIElement::zOrder);

    for (auto* child : sorted) {
        child->renderTree(rc);
    }
}

mu::Mat4x4 UIElement::buildWorldMatrix(float screenHeight) const {
    // The quad mesh spans [-1,1], so half-extents become the scale.
    // This matches the pattern in BasicPlayerHpUI and Crosshair:
    //   world = scale(...) * translate(...)
    mu::Vec3 scl{
        resolvedRect_.width * 0.5f,
        resolvedRect_.height * 0.5f,
        1.f
    };
    mu::Vec3 pos{
        resolvedRect_.x + resolvedRect_.width * 0.5f,
        screenHeight - (resolvedRect_.y + resolvedRect_.height * 0.5f),
        0.f
    };
    return mu::Mat4x4(mu::scale(scl)) * mu::translate(pos);
}

}   // namespace UI
