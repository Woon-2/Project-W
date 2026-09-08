#ifndef common_scatter_transform_hpp
#define common_scatter_transform_hpp

#include "mathUtil.hpp"

// Single source of truth for a scattered prop instance's world matrix.
//
// The client (rendering + collision) and the server (collision authority) must
// build BIT-IDENTICAL world transforms for every static prop, otherwise the
// player-vs-prop (client) and monster-vs-prop (server) resolutions reference
// different geometry and the shared player/monster contacts disagree, breaking
// position sync. Both peers ground-snap the instance to the same height field
// (same height map data) and then compose the world matrix here.
//
// Inputs are already-resolved primitives:
//   pos   - ground-snapped world position (chunk offset + local + height)
//   rot   - instance orientation (quaternion; trees yaw, details ground-align)
//   scale - per-axis world scale (mesh: (scaleW, scaleH, scaleW))
//
// Order matches the original resolveChunkScatter(): scale, then rotate, then
// translate (row-vector / DirectXMath A*B*C convention).
inline mu::Mat4x4 MU_CALLCONV makeScatterWorld(mu::Vec3 pos, mu::NQuat rot, mu::Vec3 scale) {
    return mu::scaleH(scale) * rot.mat4() * mu::translate(pos);
}

#endif // common_scatter_transform_hpp
