#include "pch.hpp"
#include "Image.hpp"
#include "../../gfx.hpp"

namespace UI {

void Image::onRender(const RenderContext& rc) {
    if (!texture) return;

    rc.gfx->addDrawEvent(UIPipeline::DrawEvent{
        .world = buildWorldMatrix(rc.screenHeight),
        .pTex = texture,
        .colorMul = colorMul,
        .uvScaleBias = uvScaleBias
    });
}

}   // namespace UI
