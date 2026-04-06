#ifndef __UI_LABEL_HPP
#define __UI_LABEL_HPP

#include "../UIElement.hpp"

struct TextImage;
struct FontHandle;

namespace UI {

class Label : public UIElement {
public:
    void setText(const std::wstring& text);
    const std::wstring& text() const { return text_; }

    void setFont(FontHandle* font) { fontHandle_ = font; dirty_ = true; }
    void setTextImage(TextImage* img) { textImage_ = img; }

    void setTextHAlign(TextHAlign a) { textHAlign_ = a; dirty_ = true; }
    void setTextVAlign(TextVAlign a) { textVAlign_ = a; dirty_ = true; }
    TextHAlign textHAlign() const { return textHAlign_; }
    TextVAlign textVAlign() const { return textVAlign_; }

    void onUpdate(const UpdateContext& ctx) override;
    void onRender(const RenderContext& rc) override;

private:
    std::wstring text_;
    std::wstring prevText_;
    FontHandle* fontHandle_ = nullptr;
    TextImage* textImage_ = nullptr;
    TextHAlign textHAlign_ = TextHAlign::Leading;
    TextVAlign textVAlign_ = TextVAlign::Top;
    bool dirty_ = true;
    bool needsCopy_ = false;
};

}   // namespace UI

#endif  // __UI_LABEL_HPP
