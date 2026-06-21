#ifndef common_arena_wall_hpp
#define common_arena_wall_hpp

#include "mathUtil.hpp"
#include "markerDef.hpp"
#include <algorithm>
#include <cmath>

// One-way arena wall (horizontal slab) derived from a "Wall" marker.
//
// A mid-boss arena is a corridor sealed by Wall markers at each end. Each wall
// blocks a committed player from crossing OUTWARD (away from the arena center)
// but lets them pass inward, so latecomers can still walk in. Enforced on the
// client (predicted local player) and mirrored by the server in Room::move().
//
// All vectors are horizontal (Y = 0): the slab only constrains XZ movement; Y
// (fall / terrain) is left untouched, matching the existing move() / separation
// clamps.
struct OneWayWall {
    mu::Vec3 center{};      // wall world position (XZ used)
    mu::Vec3 outward{};     // unit, points from arena interior toward this wall's exterior
    mu::Vec3 widthDir{};    // unit, along the wall span (perpendicular to outward, in XZ)
    float    halfWidth = 0.f;  // half of the wall's lateral span; footprint gate
};

// Builds a one-way slab from a wall marker. interiorRef is a point inside the
// arena (the midpoint of all wall markers); it picks which face is "outward".
inline OneWayWall makeOneWayWall(const MarkerDef& m, mu::Vec3 interiorRef) {
    // World directions of the marker's local X / Z axes, flattened to the XZ
    // plane (walls are upright, so their facing/span axes are horizontal).
    auto flatten = [](mu::Vec3 v) -> mu::Vec3 {
        mu::Vec3 h{ v.x(), 0.f, v.z() };
        const float len = h.len();
        return (len > 1e-4f) ? h * (1.f / len) : mu::Vec3{ 1.f, 0.f, 0.f };
    };
    const mu::Vec3 axisX = flatten(m.orient.rotate(mu::Vec3{ 1.f, 0.f, 0.f }));
    const mu::Vec3 axisZ = flatten(m.orient.rotate(mu::Vec3{ 0.f, 0.f, 1.f }));

    // m.scale is the wall's full size, so half extents = scale * 0.5. The thinner
    // horizontal axis is the wall's facing (normal); the wider one is its span.
    const float halfX = std::fabs(m.scale.x()) * 0.5f;
    const float halfZ = std::fabs(m.scale.z()) * 0.5f;

    OneWayWall w{};
    w.center = m.pos;
    mu::Vec3 normal;
    if (halfX <= halfZ) {           // thin along local X -> X is the facing normal
        normal      = axisX;
        w.widthDir  = axisZ;
        w.halfWidth = halfZ;
    } else {                        // thin along local Z -> Z is the facing normal
        normal      = axisZ;
        w.widthDir  = axisX;
        w.halfWidth = halfX;
    }

    // Orient the normal to point away from the arena interior (toward exterior).
    const mu::Vec3 toWall{ m.pos.x() - interiorRef.x(), 0.f, m.pos.z() - interiorRef.z() };
    w.outward = (normal.x() * toWall.x() + normal.z() * toWall.z() >= 0.f) ? normal
                                                                          : normal * -1.f;
    return w;
}

// One-way clamp: if the mover crossed this wall from interior to exterior within
// the wall's lateral footprint, push the new position back onto the wall plane
// (XZ only). Inward crossings and lateral movement pass through unchanged.
inline mu::Vec3 clampOneWayWall(mu::Vec3 oldPos, mu::Vec3 newPos, const OneWayWall& w, float moverRadius) {
    const mu::Vec3 dOld{ oldPos.x() - w.center.x(), 0.f, oldPos.z() - w.center.z() };
    const mu::Vec3 dNew{ newPos.x() - w.center.x(), 0.f, newPos.z() - w.center.z() };

    const float sOld = dOld.x() * w.outward.x() + dOld.z() * w.outward.z();   // signed dist along outward
    const float sNew = dNew.x() * w.outward.x() + dNew.z() * w.outward.z();
    const float lat  = std::fabs(dNew.x() * w.widthDir.x() + dNew.z() * w.widthDir.z());

    constexpr float kEps = 0.01f;
    if (sOld <= kEps && sNew > 0.f && lat <= w.halfWidth + moverRadius) {
        // Pull back to just inside the wall plane along -outward. Y untouched.
        const float push = sNew + kEps;
        return mu::Vec3{ newPos.x() - w.outward.x() * push, newPos.y(), newPos.z() - w.outward.z() * push };
    }
    return newPos;
}

#endif // common_arena_wall_hpp
