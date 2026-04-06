#ifndef __UI_PROGRESSBAR_HPP
#define __UI_PROGRESSBAR_HPP

#include "../UIElement.hpp"

namespace UI {

class ProgressBar : public UIElement {
public:
    const Texture* backgroundTex = nullptr;
    const Texture* fillTex = nullptr;

    void setProgress(float t) { progress_ = std::clamp(t, 0.f, 1.f); }
    float progress() const { return progress_; }

    void onRender(const RenderContext& rc) override;

private:
    float progress_ = 1.f;
};

}   // namespace UI

#endif  // __UI_PROGRESSBAR_HPP
