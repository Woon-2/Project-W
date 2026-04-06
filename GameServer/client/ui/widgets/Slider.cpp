#include "pch.hpp"
#include "Slider.hpp"
#include "../../gfx.hpp"

namespace UI {

void Slider::onRender(const RenderContext& rc) {
    const auto& rect = resolvedRect_;

    // Track (full width)
    if (trackTex) {
        rc.gfx->addDrawEvent(UIPipeline::DrawEvent{
            .world = buildWorldMatrix(rc.screenHeight),
            .pTex = trackTex
        });
    }

    // Handle (positioned by value)
    if (handleTex) {
        float handleHalf = handleWidthPx * 0.5f;
        float trackUsable = rect.width - handleWidthPx;
        float handleCenterX = rect.x + handleHalf + trackUsable * value;

        mu::Vec3 scl{ handleHalf, rect.height * 0.5f, 1.f };
        mu::Vec3 pos{
            handleCenterX,
            rc.screenHeight - (rect.y + rect.height * 0.5f),
            0.f
        };
        mu::Mat4x4 handleWorld = mu::Mat4x4(mu::scale(scl)) * mu::translate(pos);

        rc.gfx->addDrawEvent(UIPipeline::DrawEvent{
            .world = handleWorld,
            .pTex = handleTex
        });
    }
}

void Slider::onMouseDown(MouseButton btn, float localX, float localY) {
    if (btn == MouseButton::Left) {
        dragging_ = true;
        updateValueFromLocal(localX);
    }
}

void Slider::onMouseMove(float localX, float localY) {
    if (dragging_) {
        updateValueFromLocal(localX);
    }
}

void Slider::onMouseUp(MouseButton btn, float localX, float localY) {
    if (btn == MouseButton::Left) {
        dragging_ = false;
    }
}

void Slider::updateValueFromLocal(float localX) {
    float handleHalf = handleWidthPx * 0.5f;
    float trackUsable = resolvedRect_.width - handleWidthPx;
    if (trackUsable <= 0.f) return;

    float newVal = std::clamp((localX - handleHalf) / trackUsable, 0.f, 1.f);
    if (newVal != value) {
        value = newVal;
        if (onValueChanged) onValueChanged(value);
    }
}

}   // namespace UI
