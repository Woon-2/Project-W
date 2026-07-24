#ifndef __ui_uiShapes_HPP
#define __ui_uiShapes_HPP

// Small stateless helpers for drawing rotated solid-colour UI primitives through the
// existing UIPipeline (textured quads only). Shared by the on-screen waypoint HUD and
// the minimap direction arrow so both use one consistent arrow/beacon look.
//
// All positions are in bottom-origin pixel space (matches the UI shader: y grows up),
// the same convention MinimapHUD's quadWorld uses.

#include "../gfxUtil.hpp"   // Texture, XMFLOAT4, mu math types

class GFX;

namespace uiShapes {

    // A solid-colour quad of size w x h centred at (cx,cy), rotated CCW by angleRad
    // about its centre. tex is normally GFX::solidColorTex().
    void quad(GFX& gfx, const Texture* tex, float cx, float cy,
              float w, float h, float angleRad, const XMFLOAT4& col);

    // A 45-degree rotated square of side sizePx centred at (cx,cy) -- the beacon core.
    void diamond(GFX& gfx, const Texture* tex, float cx, float cy,
                 float sizePx, const XMFLOAT4& col);

    // A filled directional arrow (shaft + V head) of overall length ~sizePx, centred
    // at (cx,cy) and pointing along angleRad (0 = +X / screen-right).
    void arrow(GFX& gfx, const Texture* tex, float cx, float cy,
               float sizePx, float angleRad, const XMFLOAT4& col);

}  // namespace uiShapes

#endif  // __ui_uiShapes_HPP
