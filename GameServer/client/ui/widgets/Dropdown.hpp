#ifndef __UI_DROPDOWN_HPP
#define __UI_DROPDOWN_HPP

#include "../UIElement.hpp"
#include <functional>
#include <vector>
#include <string>

namespace UI {

class Label;
class Button;

// Dropdown (combobox) widget composed of existing Label and Button children.
//
// Layout:
//   [   current selection label   ][ toggle btn ]   <- header row (always visible)
//   [        item 0 button        ]               \
//   [        item 1 button        ]                |  list (visible only when open)
//   [        ...                  ]               /
//
// Call setup() to populate items before the element's first layout pass.
class Dropdown : public UIElement {
public:
    float headerHeight = 32.f;
    float itemHeight   = 28.f;
    float toggleWidth  = 32.f;

    // Colors for the white header background and the list background strip.
    XMFLOAT4 headerBgColor = { 0.95f, 0.95f, 0.95f, 1.00f };
    XMFLOAT4 listBgColor   = { 0.88f, 0.88f, 0.88f, 1.00f };

    // Fired when the user selects an item; argument is the new selectedIndex.
    std::function<void(int)> onSelectionChanged;

    void setup(std::vector<std::string> items);

    int  selectedIndex() const { return selectedIdx_; }
    bool isOpen()        const { return open_; }

    void onRender(const RenderContext& rc) override;

private:
    bool open_        = false;
    int  selectedIdx_ = 0;
    std::vector<std::string> items_;

    Label*              labelDisplay_ = nullptr;  // child: current selection text
    Button*             btnToggle_    = nullptr;  // child: blue open/close button
    UIElement*          listRoot_     = nullptr;  // child: list container
    std::vector<Button*> itemBtns_;               // grandchildren: one Button per item

    void toggle();
    void select(int idx);
    void syncVisibility();
    void syncLabel();

    mu::Mat4x4 makeSubQuadWorld(float rx, float ry, float rw, float rh,
                                float screenH) const;
};

}   // namespace UI

#endif  // __UI_DROPDOWN_HPP
