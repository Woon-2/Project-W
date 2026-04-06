#include "pch.hpp"
#include "Button.hpp"
#include "../../gfx.hpp"

namespace UI {

void Button::onRender(const RenderContext& rc) {
    const Texture* tex = nullptr;
    switch (state_) {
    case State::Normal:  tex = texNormal;  break;
    case State::Hovered: tex = texHovered ? texHovered : texNormal; break;
    case State::Pressed: tex = texPressed ? texPressed : texNormal; break;
    }
    if (!tex) return;

    rc.gfx->addDrawEvent(UIPipeline::DrawEvent{
        .world = buildWorldMatrix(rc.screenHeight),
        .pTex = tex
    });
}

void Button::onMouseEnter() {
    if (state_ != State::Pressed)
        state_ = State::Hovered;
}

void Button::onMouseLeave() {
    state_ = State::Normal;
}

void Button::onMouseDown(MouseButton btn, float localX, float localY) {
    if (btn == MouseButton::Left)
        state_ = State::Pressed;
}

void Button::onMouseUp(MouseButton btn, float localX, float localY) {
    if (btn == MouseButton::Left && state_ == State::Pressed) {
        state_ = State::Hovered;
        if (onClick) onClick();
    }
}

}   // namespace UI
