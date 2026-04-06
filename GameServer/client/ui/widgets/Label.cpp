#include "pch.hpp"
#include "Label.hpp"
#include "../../gfx.hpp"
#include "../../font.hpp"

namespace UI {

void Label::setText(const std::wstring& text) {
    if (text_ != text) {
        text_ = text;
        dirty_ = true;
    }
}

void Label::onUpdate(const UpdateContext& ctx) {
    if (!dirty_ || !textImage_ || !ctx.gfx || text_.empty()) return;

    FontHandle* font = fontHandle_ ? fontHandle_ : ctx.defaultFont;

    int outW = 0, outH = 0;
    std::ranges::fill(textImage_->pData, static_cast<BYTE>(0));
    ctx.gfx->WriteTextToBitmap(
        textImage_,
        textImage_->width, textImage_->height,
        textImage_->width * 4,
        &outW, &outH,
        font,
        text_.c_str(),
        static_cast<DWORD>(text_.size())
    );
    ctx.gfx->UpdateTextureWithTextImage(textImage_, textImage_->width, textImage_->height);

    dirty_ = false;
    needsCopy_ = true;
}

void Label::onRender(const RenderContext& rc) {
    if (!textImage_ || text_.empty()) return;

    rc.gfx->addDrawEvent(UIPipeline::DrawEvent{
        .world = buildWorldMatrix(rc.screenHeight),
        .pTex = &textImage_->texture,
        .pCopySrc = needsCopy_ ? &textImage_->textureUpload : nullptr
    });
    needsCopy_ = false;
}

}   // namespace UI
