#include "pch.hpp"
#include "TextInput.hpp"
#include "Label.hpp"
#include "../../gfx.hpp"

namespace UI {

TextInput::TextInput() {
    interactive = true;

    auto lbl = std::make_unique<Label>();
    label_ = lbl.get();
    label_->name    = "textInputLabel";
    label_->anchor  = Anchors::CenterLeft;
    label_->pivot   = Pivots::CenterLeft;
    label_->offsetX = DimValue::px(10.f);
    label_->offsetY = DimValue::px(0.f);
    label_->width   = DimValue::pct(100.f);
    label_->height  = DimValue::pct(100.f);
    label_->setTextHAlign(TextHAlign::Leading);
    label_->setTextVAlign(TextVAlign::Center);
    label_->setFontSize(22.f);
    addChild(std::move(lbl));

    refreshDisplay();
}

void TextInput::setPlaceholder(const std::wstring& s) {
    placeholder_ = s;
    refreshDisplay();
}

void TextInput::setText(const std::wstring& s) {
    text_ = s;
    if (text_.size() > maxLength_) text_.resize(maxLength_);
    refreshDisplay();
}

void TextInput::clear() {
    text_.clear();
    refreshDisplay();
}

void TextInput::setFontSize(float size) {
    if (label_) label_->setFontSize(size);
}

void TextInput::onFocus() {
    focused_ = true;
    refreshDisplay();
}

void TextInput::onBlur() {
    focused_ = false;
    refreshDisplay();
}

void TextInput::onChar(wchar_t ch) {
    // Backspace.
    if (ch == L'\b') {
        if (!text_.empty()) {
            text_.pop_back();
            refreshDisplay();
            if (onChange) onChange(text_);
        }
        return;
    }

    // Enter -> submit.
    if (ch == L'\r' || ch == L'\n') {
        if (onSubmit) onSubmit(text_);
        return;
    }

    // Ignore other control characters (tab, escape, etc.).
    if (ch < 0x20 || ch == 0x7F) return;

    if (text_.size() >= maxLength_) return;

    wchar_t c = ch;
    if (uppercase && c >= L'a' && c <= L'z') {
        c = static_cast<wchar_t>(c - (L'a' - L'A'));
    }
    if (alnumOnly) {
        const bool isAlnum = (c >= L'A' && c <= L'Z')
                          || (c >= L'a' && c <= L'z')
                          || (c >= L'0' && c <= L'9');
        if (!isAlnum) return;
    }

    text_.push_back(c);
    refreshDisplay();
    if (onChange) onChange(text_);
}

void TextInput::onRender(const RenderContext& rc) {
    const XMFLOAT4 col = focused_ ? bgColorFocused : bgColor;
    if (col.w <= 0.f) return;

    rc.gfx->addDrawEvent(UIPipeline::DrawEvent{
        .world    = buildWorldMatrix(rc.screenHeight),
        .pTex     = rc.gfx->solidColorTex(),
        .pCopySrc = nullptr,
        .colorMul = col
    });
}

void TextInput::refreshDisplay() {
    if (!label_) return;

    if (text_.empty() && !focused_) {
        // Placeholder (dimmed).
        label_->setTextColor(placeholderColor.x, placeholderColor.y, placeholderColor.z, placeholderColor.w);
        label_->setText(placeholder_.empty() ? L" " : placeholder_);
        return;
    }

    // Editing/filled: show text with a simple caret when focused.
    std::wstring shown = text_;
    if (focused_) shown += L"|";
    label_->setTextColor(textColor.x, textColor.y, textColor.z, textColor.w);
    label_->setText(shown.empty() ? L" " : shown);
}

}   // namespace UI
