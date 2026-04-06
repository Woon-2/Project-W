#ifndef __UI_LABEL_HPP
#define __UI_LABEL_HPP

#include "../UIElement.hpp"
#include "../../font.hpp"

struct TextImage;

namespace UI {

class Label : public UIElement {
public:
    void setText(const std::wstring& text);
    const std::wstring& text() const { return text_; }

    void setFont(FontHandle* font) { fontHandle_ = font; dirty_ = true; }
    void setTextImage(TextImage* img) { textImage_ = img; }

    // 0 = 기본 폰트(16pt Tahoma) 사용. autoSize가 활성화되면 무시됨.
    void setFontSize(float size);
    // enabled=true이면 텍스트가 TextImage 크기에 꽉 차도록 폰트 크기를 자동 결정.
    void setAutoSize(bool enabled, float minSize = 8.f, float maxSize = 72.f);

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

    FontHandle ownedFont_{};
    float      fontSize_     = 0.f;
    float      prevFontSize_ = -1.f;
    bool       autoSize_     = false;
    float      autoSizeMin_  = 8.f;
    float      autoSizeMax_  = 72.f;
};

}   // namespace UI

#endif  // __UI_LABEL_HPP
