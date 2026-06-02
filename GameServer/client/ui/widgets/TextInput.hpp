#ifndef __UI_TEXTINPUT_HPP
#define __UI_TEXTINPUT_HPP

// No Korean comments in new files (encoding issue per CLAUDE.md).

#include "../UIElement.hpp"
#include <functional>
#include <string>

namespace UI {

class Label;

// Single-line text input field.
// - Receives character input (WM_CHAR) while focused (click to focus).
// - Backspace edits, Enter submits.
// - Text is displayed through an internal child Label; this widget only draws
//   the background quad and manages the edit buffer.
class TextInput : public UIElement {
public:
    TextInput();

    void setPlaceholder(const std::wstring& s);
    void setText(const std::wstring& s);
    void clear();
    const std::wstring& text() const { return text_; }

    void setMaxLength(std::size_t n) { maxLength_ = n; }
    void setFontSize(float size);

    // Optional 9-slice background texture (overrides the solid-color quad when set).
    // sliceCornerX/Y > 0 enables 9-slice; uvBorderX/Y are slice insets (fraction of source).
    const Texture* backgroundTex = nullptr;
    float sliceUvBorderX = 0.f;
    float sliceUvBorderY = 0.f;
    float sliceCornerX   = 0.f;
    float sliceCornerY   = 0.f;
    // Color multiplier applied to the texture per state.
    XMFLOAT4 texTint        = { 1.f, 1.f, 1.f, 1.f };
    XMFLOAT4 texTintFocused = { 1.f, 1.f, 1.f, 1.f };

    // Solid-color backgrounds (alpha 0 = no quad).
    XMFLOAT4 bgColor        = { 0.10f, 0.10f, 0.14f, 0.95f };
    XMFLOAT4 bgColorFocused = { 0.16f, 0.18f, 0.26f, 1.00f };
    XMFLOAT4 textColor        = { 1.00f, 1.00f, 1.00f, 1.00f };
    XMFLOAT4 placeholderColor = { 0.50f, 0.50f, 0.50f, 1.00f };

    // Input filters.
    bool uppercase = false;   // transform letters to upper-case
    bool alnumOnly = false;   // accept only [A-Za-z0-9]

    // Callbacks.
    std::function<void(const std::wstring&)> onChange;   // text changed
    std::function<void(const std::wstring&)> onSubmit;   // Enter pressed

    void onRender(const RenderContext& rc) override;
    void onChar(wchar_t ch) override;
    void onFocus() override;
    void onBlur() override;

private:
    void refreshDisplay();

    std::wstring text_;
    std::wstring placeholder_;
    std::size_t  maxLength_ = 32;
    bool         focused_   = false;
    Label*       label_     = nullptr;   // child, owned by UIElement children
};

}   // namespace UI

#endif  // __UI_TEXTINPUT_HPP
