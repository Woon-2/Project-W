#ifndef __UI_BUILD_HPP
#define __UI_BUILD_HPP

// No Korean comments in new files (encoding issue per CLAUDE.md).
//
// Small inline widget builders shared by composite UI screens (LobbyUI,
// SettingsPanel). They wrap the verbose addChild + applyRect + style boilerplate
// so screen code stays declarative. All coordinates are top-left pixel offsets
// relative to the parent element.

#include "UIElement.hpp"
#include "widgets/Button.hpp"
#include "widgets/Label.hpp"

#include <functional>
#include <string>

namespace UI::Build {

// Set anchor/pivot + top-left pixel rect in one shot.
inline void applyRect(UIElement* e, Anchor anchor, Pivot pivot,
                      float x, float y, float w, float h) {
    e->anchor  = anchor;
    e->pivot   = pivot;
    e->offsetX = DimValue::px(x);
    e->offsetY = DimValue::px(y);
    e->width   = DimValue::px(w);
    e->height  = DimValue::px(h);
}

// Non-interactive solid color block (top-left anchored).
inline Button* addSolid(UIElement* parent, const std::string& name,
                        float x, float y, float w, float h,
                        const XMFLOAT4& color, int zOrder = 0) {
    auto* block = static_cast<Button*>(parent->addChild(std::make_unique<Button>()));
    block->name = name;
    applyRect(block, Anchors::TopLeft, Pivots::TopLeft, x, y, w, h);
    block->interactive = false;
    block->bgColor     = color;
    block->zOrder      = zOrder;
    return block;
}

// Text label (top-left anchored).
inline Label* addLabel(UIElement* parent, const std::wstring& text,
                       float x, float y, float fontSize, float w, float h,
                       const XMFLOAT4& color, TextHAlign hAlign = TextHAlign::Leading,
                       int zOrder = 1) {
    auto* lbl = static_cast<Label*>(parent->addChild(std::make_unique<Label>()));
    applyRect(lbl, Anchors::TopLeft, Pivots::TopLeft, x, y, w, h);
    lbl->setTextHAlign(hAlign);
    lbl->setTextVAlign(TextVAlign::Center);
    lbl->setFontSize(fontSize);
    lbl->setTextColor(color.x, color.y, color.z, color.w);
    lbl->setText(text);
    lbl->zOrder = zOrder;
    return lbl;
}

// Button with a centered caption label as its single child.
inline Button* addButton(UIElement* parent, const std::wstring& text,
                         float x, float y, float w, float h,
                         const XMFLOAT4& normal, const XMFLOAT4& hovered, const XMFLOAT4& pressed,
                         float fontSize, std::function<void()> onClick,
                         const XMFLOAT4& textColor) {
    auto* btn = static_cast<Button*>(parent->addChild(std::make_unique<Button>()));
    applyRect(btn, Anchors::TopLeft, Pivots::TopLeft, x, y, w, h);
    btn->bgColor        = normal;
    btn->bgColorHovered = hovered;
    btn->bgColorPressed = pressed;
    btn->onClick        = std::move(onClick);
    btn->zOrder         = 1;

    auto* lbl = static_cast<Label*>(btn->addChild(std::make_unique<Label>()));
    applyRect(lbl, Anchors::TopLeft, Pivots::TopLeft, 0.f, 0.f, w, h);
    lbl->setTextHAlign(TextHAlign::Center);
    lbl->setTextVAlign(TextVAlign::Center);
    lbl->setFontSize(fontSize);
    lbl->setTextColor(textColor.x, textColor.y, textColor.z, textColor.w);
    lbl->setText(text);
    return btn;
}

}   // namespace UI::Build

#endif  // __UI_BUILD_HPP
