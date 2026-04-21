#include "pch.hpp"
#include "ProgressBar.hpp"
#include "../../gfx.hpp"

namespace UI {

void ProgressBar::onRender(const RenderContext& rc) {
    const auto& rect = resolvedRect_;

    // Background (full width)
    {
        const Texture* tex = backgroundTex ? backgroundTex : rc.gfx->solidColorTex();
        XMFLOAT4 col = backgroundTex ? XMFLOAT4{ 1.f, 1.f, 1.f, 1.f } : bgColor;
        rc.gfx->addDrawEvent(UIPipeline::DrawEvent{
            .world    = buildWorldMatrix(rc.screenHeight),
            .pTex     = tex,
            .pCopySrc = nullptr,
            .colorMul = col
        });
    }

    // Fill bar (scaled by progress, anchored to left edge)
    if (progress_ > 0.f) {
        const Texture* tex = fillTex ? fillTex : rc.gfx->solidColorTex();
        XMFLOAT4 col = fillTex ? XMFLOAT4{ 1.f, 1.f, 1.f, 1.f } : fillColor;

        mu::Vec3 scl{
            rect.width * 0.5f * progress_,
            rect.height * 0.5f,
            1.f
        };
        mu::Vec3 pos{
            rect.x + rect.width * 0.5f * progress_,
            rc.screenHeight - (rect.y + rect.height * 0.5f),
            0.f
        };
        mu::Mat4x4 fillWorld = mu::Mat4x4(mu::scale(scl)) * mu::translate(pos);

        rc.gfx->addDrawEvent(UIPipeline::DrawEvent{
            .world    = fillWorld,
            .pTex     = tex,
            .pCopySrc = nullptr,
            .colorMul = col
        });
    }
}

}   // namespace UI
