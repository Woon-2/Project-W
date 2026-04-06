#include "pch.hpp"
#include "ProgressBar.hpp"
#include "../../gfx.hpp"

namespace UI {

void ProgressBar::onRender(const RenderContext& rc) {
    const auto& rect = resolvedRect_;

    // Background (full width)
    if (backgroundTex) {
        rc.gfx->addDrawEvent(UIPipeline::DrawEvent{
            .world = buildWorldMatrix(rc.screenHeight),
            .pTex = backgroundTex
        });
    }

    // Fill bar (scaled by progress, anchored to left edge)
    // Matches BasicPlayerHpUI pattern:
    //   scaleX = halfWidth * progress
    //   translateX = center shifted left by unfilled portion
    if (fillTex && progress_ > 0.f) {
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
            .world = fillWorld,
            .pTex = fillTex
        });
    }
}

}   // namespace UI
