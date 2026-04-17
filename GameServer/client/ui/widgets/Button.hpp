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
