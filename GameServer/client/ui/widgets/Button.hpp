#ifndef __UI_BUTTON_HPP
#define __UI_BUTTON_HPP

#include "../UIElement.hpp"

namespace UI {

class Button : public UIElement {
public:
    Button() { interactive = true; }

    const Texture* texNormal  = nullptr;
    const Texture* texHovered = nullptr;
    const Texture* texPressed = nullptr;

    // 9-slice for the textured path. sliceCornerX/Y > 0 enables it; otherwise the
    // texture is stretched. uvBorderX/Y are the slice insets (fraction of source).
    float sliceUvBorderX = 0.f;
    float sliceUvBorderY = 0.f;
    float sliceCornerX   = 0.f;
    float sliceCornerY   = 0.f;

    // Per-state color multiplier applied to the texture (not the solid fallback).
    XMFLOAT4 texTint        = { 1.f,  1.f,  1.f,  1.f };
    XMFLOAT4 texTintHovered = { 1.12f, 1.12f, 1.12f, 1.f };
    XMFLOAT4 texTintPressed = { 0.82f, 0.82f, 0.82f, 1.f };

    // Solid-color fallback used when the corresponding texture is nullptr.
    // Alpha = 0 means transparent (no quad submitted).
    XMFLOAT4 bgColor        = { 1.f, 1.f, 1.f, 0.f };
    XMFLOAT4 bgColorHovered = { 0.f, 0.f, 0.f, 0.f }; // 0 = inherit bgColor
    XMFLOAT4 bgColorPressed = { 0.f, 0.f, 0.f, 0.f }; // 0 = inherit bgColor

    std::function<void()> onClick;

    void onRender(const RenderContext& rc) override;
    void onMouseEnter() override;
    void onMouseLeave() override;
    void onMouseDown(MouseButton btn, float localX, float localY) override;
    void onMouseUp(MouseButton btn, float localX, float localY) override;

private:
    enum class State { Normal, Hovered, Pressed };
    State state_ = State::Normal;
};

}   // namespace UI

#endif  // __UI_BUTTON_HPP
