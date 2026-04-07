#ifndef __UI_SLIDER_HPP
#define __UI_SLIDER_HPP

#include "../UIElement.hpp"

namespace UI {

class Slider : public UIElement {
public:
    Slider() { interactive = true; }

    const Texture* trackTex  = nullptr;
    const Texture* handleTex = nullptr;

    float value = 0.5f;
    float handleWidthPx = 16.f;

    std::function<void(float)> onValueChanged;

    void onRender(const RenderContext& rc) override;
    void onMouseDown(MouseButton btn, float localX, float localY) override;
    void onMouseMove(float localX, float localY) override;
    void onMouseUp(MouseButton btn, float localX, float localY) override;

private:
    bool dragging_ = false;
    void updateValueFromLocal(float localX);
};

}   // namespace UI

#endif  // __UI_SLIDER_HPP
