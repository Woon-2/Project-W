#include "pch.hpp"
#include "UIElement.hpp"
#include "../gfx.hpp"

namespace UI {

void emitNineSlice(const RenderContext& rc, const Rect& rect, const Texture* tex,
                   float uvBorderX, float uvBorderY,
                   float cornerX, float cornerY,
                   const XMFLOAT4& colorMul) {
    if (!tex) return;

    cornerX *= rc.uiScale;
    cornerY *= rc.uiScale;

    // Clamp so opposite corners never overlap, then snap edges to whole pixels
    // (shared rounded edges keep adjacent slices seamless).
    cornerX = std::min(cornerX, rect.width  * 0.5f);
    cornerY = std::min(cornerY, rect.height * 0.5f);
    uvBorderX = std::clamp(uvBorderX, 0.f, 0.5f);
    uvBorderY = std::clamp(uvBorderY, 0.f, 0.5f);

    const float xs[4] = {
        std::round(rect.x),
        std::round(rect.x + cornerX),
        std::round(rect.x + rect.width - cornerX),
        std::round(rect.x + rect.width)
    };
    const float ys[4] = {
        std::round(rect.y),
        std::round(rect.y + cornerY),
        std::round(rect.y + rect.height - cornerY),
        std::round(rect.y + rect.height)
    };
    const float us[4] = { 0.f, uvBorderX, 1.f - uvBorderX, 1.f };
    const float vs[4] = { 0.f, uvBorderY, 1.f - uvBorderY, 1.f };

    for (int row = 0; row < 3; ++row) {
        const float cy = ys[row];
        const float ch = ys[row + 1] - ys[row];
        if (ch <= 0.f) continue;

        for (int col = 0; col < 3; ++col) {
            const float cx = xs[col];
            const float cw = xs[col + 1] - xs[col];
            if (cw <= 0.f) continue;

            mu::Vec3 scl{ cw * 0.5f, ch * 0.5f, 1.f };
            mu::Vec3 pos{ cx + cw * 0.5f, rc.screenHeight - (cy + ch * 0.5f), 0.f };
            mu::Mat4x4 world = mu::Mat4x4(mu::scale(scl)) * mu::translate(pos);

            rc.gfx->addDrawEvent(UIPipeline::DrawEvent{
                .world       = world,
                .pTex        = tex,
                .pCopySrc    = nullptr,
                .colorMul    = colorMul,
                .uvScaleBias = XMFLOAT4{ us[col + 1] - us[col], vs[row + 1] - vs[row], us[col], vs[row] }
            });
        }
    }
}

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

void UIElement::layout(const Rect& parentRect, float pixelScale) {
    float refX = parentRect.x + anchor.x * parentRect.width;
    float refY = parentRect.y + anchor.y * parentRect.height;

    float w = width.resolve(parentRect.width, pixelScale);
    float h = height.resolve(parentRect.height, pixelScale);

    float ox = offsetX.resolve(parentRect.width, pixelScale);
    float oy = offsetY.resolve(parentRect.height, pixelScale);

    resolvedRect_.x = refX + ox - pivot.x * w;
    resolvedRect_.y = refY + oy - pivot.y * h;
    resolvedRect_.width = w;
    resolvedRect_.height = h;

    for (auto& child : children_) {
        child->layout(resolvedRect_, pixelScale);
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
    const float rw = std::round(resolvedRect_.width);
    const float rh = std::round(resolvedRect_.height);
    mu::Vec3 scl{
        rw * 0.5f,
        rh * 0.5f,
        1.f
    };
    mu::Vec3 pos{
        std::round(resolvedRect_.x + resolvedRect_.width  * 0.5f),
        std::round(screenHeight - (resolvedRect_.y + resolvedRect_.height * 0.5f)),
        0.f
    };
    return mu::Mat4x4(mu::scale(scl)) * mu::translate(pos);
}

}   // namespace UI
