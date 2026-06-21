#ifndef __UI_PANEL_HPP
#define __UI_PANEL_HPP

#include "../UIElement.hpp"

namespace UI {

class Panel : public UIElement {
public:
    const Texture* backgroundTex = nullptr;
    bool drawSolidBackground = false;

    void onRender(const RenderContext& rc) override;
};

}   // namespace UI

#endif  // __UI_PANEL_HPP
