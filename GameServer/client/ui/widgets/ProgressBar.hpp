#ifndef __UI_PROGRESSBAR_HPP
#define __UI_PROGRESSBAR_HPP

#include "../UIElement.hpp"

namespace UI {

class ProgressBar : public UIElement {
public:
    const Texture* backgroundTex = nullptr;
    const Texture* fillTex       = nullptr;

    // Solid colors used when the corresponding texture is nullptr.
    XMFLOAT4 bgColor   = { 0.2f, 0.2f, 0.2f, 1.f };
    XMFLOAT4 fillColor = { 0.f,  0.8f, 0.f,  1.f };

    void setProgress(float t) { progress_ = std::clamp(t, 0.f, 1.f); }
    float progress() const { return progress_; }

    void onRender(const RenderContext& rc) override;

private:
    float progress_ = 1.f;
};

}   // namespace UI

#endif  // __UI_PROGRESSBAR_HPP
