#include "pch.hpp"
#include "Panel.hpp"
#include "../../gfx.hpp"

namespace UI {

void Panel::onRender(const RenderContext& rc) {
    if (!backgroundTex) return;

    rc.gfx->addDrawEvent(UIPipeline::DrawEvent{
        .world = buildWorldMatrix(rc.screenHeight),
        .pTex = backgroundTex
    });
}

}   // namespace UI
