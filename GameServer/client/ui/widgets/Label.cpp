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

void Label::setFontSize(float size) {
    if (fontSize_ != size) { fontSize_ = size; dirty_ = true; }
}

void Label::setAutoSize(bool enabled, float minSize, float maxSize) {
    autoSize_    = enabled;
    autoSizeMin_ = minSize;
    autoSizeMax_ = maxSize;
    dirty_ = true;
}

void Label::onUpdate(const UpdateContext& ctx) {
    if (!dirty_ || !textImage_ || !ctx.gfx || text_.empty()) return;

    FontHandle* font = nullptr;

    if (autoSize_) {
        float lo = autoSizeMin_, hi = autoSizeMax_;
        for (int i = 0; i < 8; ++i) {
            float mid = (lo + hi) * 0.5f;
            FontHandle tryFont = ctx.gfx->createFont(mid);
            int tw = 0, th = 0;
            ctx.gfx->measureText(&tryFont, text_.c_str(), static_cast<DWORD>(text_.size()),
                                 static_cast<float>(textImage_->width),
                                 static_cast<float>(textImage_->height),
                                 &tw, &th);
            if (tw <= static_cast<int>(textImage_->width) && th <= static_cast<int>(textImage_->height))
                lo = mid;
            else
                hi = mid;
        }
        if (lo != prevFontSize_) {
            ownedFont_    = ctx.gfx->createFont(lo);
            prevFontSize_ = lo;
        }
        font = &ownedFont_;
    } else if (fontSize_ > 0.f) {
        if (fontSize_ != prevFontSize_) {
            ownedFont_    = ctx.gfx->createFont(fontSize_);
            prevFontSize_ = fontSize_;
        }
        font = &ownedFont_;
    } else {
        font = fontHandle_ ? fontHandle_ : ctx.defaultFont;
    }

    if (!font) return;

    // WriteTextToBitmap always renders text at (0,0) in the bitmap (LEADING/NEAR).
    // After the write, we shift the pixels within pData to the aligned position
    // so the full-size quad shows text in the correct location.

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

    // Shift pixels within pData to implement alignment.
    if (outW > 0 && outH > 0) {
        int destX = 0, destY = 0;
        const int imgW = static_cast<int>(textImage_->width);
        const int imgH = static_cast<int>(textImage_->height);
        if (textHAlign_ == TextHAlign::Center)   destX = (imgW - outW) / 2;
        if (textHAlign_ == TextHAlign::Trailing) destX =  imgW - outW;
        if (textVAlign_ == TextVAlign::Center)   destY = (imgH - outH) / 2;
        if (textVAlign_ == TextVAlign::Bottom)   destY =  imgH - outH;

        if (destX != 0 || destY != 0) {
            const int stride = imgW * 4;
            std::vector<BYTE> shifted(textImage_->pData.size(), 0);
            for (int row = 0; row < outH; ++row) {
                const int dstRow = destY + row;
                if (dstRow < 0 || dstRow >= imgH) continue;
                const int srcOff = row    * stride;
                const int dstOff = dstRow * stride + destX * 4;
                const int copyBytes = std::min(outW * 4, stride - destX * 4);
                if (copyBytes > 0)
                    memcpy(shifted.data() + dstOff, textImage_->pData.data() + srcOff, copyBytes);
            }
            textImage_->pData = std::move(shifted);
        }
    }

    ctx.gfx->UpdateTextureWithTextImage(textImage_, textImage_->width, textImage_->height);
    dirty_ = false;
    needsCopy_ = true;
}

void Label::onRender(const RenderContext& rc) {
    if (!textImage_ || text_.empty()) return;

    rc.gfx->addDrawEvent(UIPipeline::DrawEvent{
        .world    = buildWorldMatrix(rc.screenHeight),
        .pTex     = &textImage_->texture,
        .pCopySrc = needsCopy_ ? &textImage_->textureUpload : nullptr
    });
    needsCopy_ = false;
}

}   // namespace UI
