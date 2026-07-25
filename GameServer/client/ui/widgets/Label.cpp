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

void Label::setFontFamily(std::wstring family) {
    if (family.empty()) family = L"Tahoma";
    if (fontFamily_ != family) {
        fontFamily_ = std::move(family);
        prevFontSize_ = -1.f;
        dirty_ = true;
    }
}

void Label::setFontWeight(DWRITE_FONT_WEIGHT weight) {
    if (fontWeight_ != weight) {
        fontWeight_ = weight;
        prevFontSize_ = -1.f;
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
    if (!ctx.gfx || text_.empty()) return;

    // resolvedRect_ 크기에 맞게 TextImage를 자동 생성/재생성
    const UINT texW = static_cast<UINT>(std::max(1.f, std::round(resolvedRect_.width)));
    const UINT texH = static_cast<UINT>(std::max(1.f, std::round(resolvedRect_.height)));
    if (texW != prevTexW_ || texH != prevTexH_) {
        ctx.gfx->createTextImageImmediate(texW, texH, &ownedTextImage_);
        prevTexW_ = texW;
        prevTexH_ = texH;
        dirty_ = true;
    }

    if (!dirty_ || ownedTextImage_.pData.empty()) return;

    FontHandle* font = nullptr;

    if (autoSize_) {
        float lo = autoSizeMin_ * ctx.uiScale;
        float hi = autoSizeMax_ * ctx.uiScale;
        for (int i = 0; i < 8; ++i) {
            float mid = (lo + hi) * 0.5f;
            FontHandle tryFont = ctx.gfx->createFont(fontFamily_.c_str(), mid, fontWeight_);
            int tw = 0, th = 0;
            ctx.gfx->measureText(&tryFont, text_.c_str(), static_cast<DWORD>(text_.size()),
                                 static_cast<float>(ownedTextImage_.width),
                                 static_cast<float>(ownedTextImage_.height),
                                 &tw, &th);
            if (tw <= static_cast<int>(ownedTextImage_.width) && th <= static_cast<int>(ownedTextImage_.height))
                lo = mid;
            else
                hi = mid;
        }
        if (lo != prevFontSize_) {
            ownedFont_    = ctx.gfx->createFont(fontFamily_.c_str(), lo, fontWeight_);
            prevFontSize_ = lo;
        }
        font = &ownedFont_;
    } else if (fontSize_ > 0.f) {
        const float scaledFontSize = fontSize_ * ctx.uiScale;
        if (scaledFontSize != prevFontSize_) {
            ownedFont_    = ctx.gfx->createFont(fontFamily_.c_str(), scaledFontSize, fontWeight_);
            prevFontSize_ = scaledFontSize;
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
    std::ranges::fill(ownedTextImage_.pData, static_cast<BYTE>(0));
    if (!ctx.gfx->WriteTextToBitmap(
        &ownedTextImage_,
        ownedTextImage_.width, ownedTextImage_.height,
        ownedTextImage_.width * 4,
        &outW, &outH,
        font,
        text_.c_str(),
        static_cast<DWORD>(text_.size()),
        textColor_
    )) {
        // Text rendering failed (e.g. transient device loss). Keep dirty_ so the
        // label retries next frame; skip the upload so the previous texture stays.
        return;
    }

    // Shift pixels within pData to implement alignment.
    if (outW > 0 && outH > 0) {
        int destX = 0, destY = 0;
        const int imgW = static_cast<int>(ownedTextImage_.width);
        const int imgH = static_cast<int>(ownedTextImage_.height);
        if (textHAlign_ == TextHAlign::Center)   destX = (imgW - outW) / 2;
        if (textHAlign_ == TextHAlign::Trailing) destX =  imgW - outW;
        if (textVAlign_ == TextVAlign::Center)   destY = (imgH - outH) / 2;
        if (textVAlign_ == TextVAlign::Bottom)   destY =  imgH - outH;

        if (destX != 0 || destY != 0) {
            const int stride = imgW * 4;
            std::vector<BYTE> shifted(ownedTextImage_.pData.size(), 0);
            for (int row = 0; row < outH; ++row) {
                const int dstRow = destY + row;
                if (dstRow < 0 || dstRow >= imgH) continue;
                const int srcOff = row    * stride;
                const int dstOff = dstRow * stride + destX * 4;
                const int copyBytes = std::min(outW * 4, stride - destX * 4);
                if (copyBytes > 0)
                    memcpy(shifted.data() + dstOff, ownedTextImage_.pData.data() + srcOff, copyBytes);
            }
            ownedTextImage_.pData = std::move(shifted);
        }
    }

    ctx.gfx->UpdateTextureWithTextImage(&ownedTextImage_, ownedTextImage_.width, ownedTextImage_.height);
    dirty_ = false;
    needsCopy_ = true;
}

void Label::onRender(const RenderContext& rc) {
    if (ownedTextImage_.pData.empty() || text_.empty()) return;

    rc.gfx->addDrawEvent(UIPipeline::DrawEvent{
        .world    = buildWorldMatrix(rc.screenHeight),
        .pTex     = &ownedTextImage_.texture,
        .pCopySrc = needsCopy_ ? &ownedTextImage_.textureUpload : nullptr,
        .colorMul = XMFLOAT4{ colorTint.r, colorTint.g, colorTint.b, colorTint.a }
    });
    needsCopy_ = false;
}

}   // namespace UI
