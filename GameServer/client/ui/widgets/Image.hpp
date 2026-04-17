#ifndef __UI_IMAGE_HPP
#define __UI_IMAGE_HPP

#include "../UIElement.hpp"

namespace UI {

class Image : public UIElement {
public:
    const Texture* texture = nullptr;

    void onRender(const RenderContext& rc) override;
};

}   // namespace UI

#endif  // __UI_IMAGE_HPP
