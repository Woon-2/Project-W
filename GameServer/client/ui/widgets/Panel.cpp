#include "pch.hpp"
#include "Panel.hpp"
#include "../../gfx.hpp"

namespace UI {

void Panel::onRender(const RenderContext& rc) {
    if (!backgroundTex && !drawSolidBackground) return;
    const Texture* tex = backgroundTex ? backgroundTex : rc.gfx->solidColorTex();

    rc.gfx->addDrawEvent(UIPipeline::DrawEvent{
        .world = buildWorldMatrix(rc.screenHeight),
        .pTex = tex,
        .colorMul = XMFLOAT4{ colorTint.r, colorTint.g, colorTint.b, colorTint.a }
    });
}

}   // namespace UI
