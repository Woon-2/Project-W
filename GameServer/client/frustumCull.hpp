#ifndef __frustumCull_HPP
#define __frustumCull_HPP

#include <array>
#include <cmath>
#include "collision.hpp"   // AABB (+ mu:: math types via pch)

// ----------------------------------------------------------------------------
// View-frustum culling helpers.
//
// Shared by per-instance scatter prop culling. Extracted as a small reusable
// utility (Gribb-Hartmann plane extraction + plane/AABB test) so callers do not
// re-derive the same projection math inline. The existing per-object cullObjects()
// paths keep their own inline implementation and are intentionally left untouched.
//
// DirectXMath row-vector convention: v' = v * M, so a clip-space component j is
// dot(P, col(j)). A point P is inside a plane when dot(plane.xyz, P) + plane.w >= 0.
// ----------------------------------------------------------------------------

struct Frustum {
    // left, right, bottom, top, near, far
    std::array<mu::Vec4, 6> planes{};
};

inline Frustum extractFrustum(const mu::Mat4x4& viewProj) {
    const auto c0 = viewProj.col(0);
    const auto c1 = viewProj.col(1);
    const auto c2 = viewProj.col(2);
    const auto c3 = viewProj.col(3);

    return Frustum{ {
        c3 + c0,   // left
        c3 - c0,   // right
        c3 + c1,   // bottom
        c3 - c1,   // top
        c2,        // near
        c3 - c2,   // far
    } };
}

// Returns true if the world-space AABB is at least partially inside the frustum
// (i.e. it should NOT be culled). Uses the AABB projection-radius test, so it is
// conservative: it only reports "outside" when the box is fully beyond a plane.
inline bool intersects(const Frustum& f, const AABB& aabb) {
    const mu::Vec3 c = aabb.center;
    const mu::Vec3 h = aabb.size * 0.5f;

    for (const auto& pl : f.planes) {
        const float e = std::abs(pl.x()) * h.x()
                      + std::abs(pl.y()) * h.y()
                      + std::abs(pl.z()) * h.z();
        const float dist = pl.x() * c.x() + pl.y() * c.y()
                         + pl.z() * c.z() + pl.w();
        if (dist + e < 0.f) return false;
    }
    return true;
}

// OBB variant: same projection-radius test, but the box half-extents are projected
// onto each plane normal through the OBB's own oriented axes. Conservative.
inline bool intersects(const Frustum& f, const OBB& obb) {
    const mu::Vec3 c  = obb.center;
    const mu::Vec3 ax = obb.orient.rotate(mu::Vec3(1.f, 0.f, 0.f));
    const mu::Vec3 ay = obb.orient.rotate(mu::Vec3(0.f, 1.f, 0.f));
    const mu::Vec3 az = obb.orient.rotate(mu::Vec3(0.f, 0.f, 1.f));

    for (const auto& pl : f.planes) {
        const float e =
            std::abs(pl.x()*ax.x() + pl.y()*ax.y() + pl.z()*ax.z()) * obb.halfExtents.x()
          + std::abs(pl.x()*ay.x() + pl.y()*ay.y() + pl.z()*ay.z()) * obb.halfExtents.y()
          + std::abs(pl.x()*az.x() + pl.y()*az.y() + pl.z()*az.z()) * obb.halfExtents.z();
        const float dist = pl.x()*c.x() + pl.y()*c.y() + pl.z()*c.z() + pl.w();
        if (dist + e < 0.f) return false;
    }
    return true;
}

#endif  // __frustumCull_HPP
