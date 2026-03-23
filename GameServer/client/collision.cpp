#include "pch.hpp"
#include "collision.hpp"

CollisionResult collides(const AABB& a, const AABB& b) {
    CollisionResult res{ .hit = false };

    mu::Vec3 halfA = a.size * 0.5f;
    mu::Vec3 halfB = b.size * 0.5f;

    mu::Vec3 delta = b.center - a.center;
    mu::Vec3 overlap = (halfA + halfB) - mu::abs(delta);

    if (overlap.x() < 0 || overlap.y() < 0 || overlap.z() < 0) {
        return res;
    }

    res.hit = true;

    if (overlap.x() < overlap.y() && overlap.x() < overlap.z()) {
        res.depth = overlap.x();
        res.normal = mu::NVec3((delta.x() > 0) ? -1.0f : 1.0f, 0, 0);
        res.mtv = mu::Vec3(res.normal) * res.depth;
    }
    else if (overlap.y() < overlap.z()) {
        res.depth = overlap.y();
        res.normal = mu::NVec3(0, (delta.y() > 0) ? -1.0f : 1.0f, 0);
        res.mtv = mu::Vec3(res.normal) * res.depth;
    }
    else {
        res.depth = overlap.z();
        res.normal = mu::Vec3(0, 0, (delta.z() > 0) ? -1.0f : 1.0f);
        res.mtv = mu::Vec3(res.normal) * res.depth;
    }

    mu::Vec3 minA = a.center - halfA;
    mu::Vec3 maxA = a.center + halfA;
    mu::Vec3 minB = b.center - halfB;
    mu::Vec3 maxB = b.center + halfB;

    res.contactPoint = (mu::max(minA, minB) + mu::min(maxA, maxB)) * 0.5f;

    return res;
}

// --- OBB helpers ---

static std::array<mu::Vec3, 3> obbLocalAxes(const OBB& obb) {
    return {
        obb.orient.rotate(mu::Vec3(1.f, 0.f, 0.f)),
        obb.orient.rotate(mu::Vec3(0.f, 1.f, 0.f)),
        obb.orient.rotate(mu::Vec3(0.f, 0.f, 1.f)),
    };
}

// Projects OBB half-extents onto a separation axis.
static float projectOBBOntoAxis(
    const OBB& obb, const std::array<mu::Vec3, 3>& axes, mu::Vec3 axis
) {
    return std::abs(mu::dot(axes[0], axis)) * obb.halfExtents.x()
         + std::abs(mu::dot(axes[1], axis)) * obb.halfExtents.y()
         + std::abs(mu::dot(axes[2], axis)) * obb.halfExtents.z();
}

// OBB vs OBB: separating axis theorem with 15 candidate axes.
CollisionResult collides(const OBB& a, const OBB& b) {
    CollisionResult res{ .hit = false };

    const auto axesA = obbLocalAxes(a);
    const auto axesB = obbLocalAxes(b);
    const mu::Vec3 d = b.center - a.center;

    mu::Vec3 candAxes[15];
    candAxes[0] = axesA[0]; candAxes[1] = axesA[1]; candAxes[2] = axesA[2];
    candAxes[3] = axesB[0]; candAxes[4] = axesB[1]; candAxes[5] = axesB[2];
    int idx = 6;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            candAxes[idx++] = mu::cross(axesA[i], axesB[j]);
        }
    }

    float     minOverlap = std::numeric_limits<float>::max();
    mu::Vec3  bestAxis{};

    for (int k = 0; k < 15; ++k) {
        const float axisLen2 = candAxes[k].len2();
        if (axisLen2 < 1e-10f) continue;   // degenerate cross product (parallel edges)

        const mu::Vec3 axis = mu::Vec3(mu::normalize(candAxes[k]));

        const float rA      = projectOBBOntoAxis(a, axesA, axis);
        const float rB      = projectOBBOntoAxis(b, axesB, axis);
        const float dProj   = mu::dot(d, axis);
        const float overlap = rA + rB - std::abs(dProj);

        if (overlap < 0.f) return res;   // separating axis found — no collision

        if (overlap < minOverlap) {
            minOverlap = overlap;
            const float sign = (dProj >= 0.f) ? 1.f : -1.f;
            bestAxis = axis * sign;
        }
    }

    res.hit          = true;
    res.depth        = minOverlap;
    res.normal       = mu::NVec3(bestAxis, mu::NVec3::NoNormalize_t{});
    res.mtv          = bestAxis * minOverlap;
    res.contactPoint = (a.center + b.center) * 0.5f;

    return res;
}

OBB toOBB(const AABB& aabb) {
    return OBB{
        .center      = aabb.center,
        .halfExtents = aabb.size * 0.5f,
        .orient      = mu::NQuat{},
    };
}

CollisionResult collides(const CollisionVolume& a, const CollisionVolume& b) {
    return std::visit([](auto&& va, auto&& vb) -> CollisionResult {
        using A = std::decay_t<decltype(va)>;
        using B = std::decay_t<decltype(vb)>;
        if constexpr (std::is_same_v<A, AABB> && std::is_same_v<B, AABB>) {
            return collides(va, vb);
        } else if constexpr (std::is_same_v<A, OBB> && std::is_same_v<B, OBB>) {
            return collides(va, vb);
        } else if constexpr (std::is_same_v<A, AABB>) {
            return collides(toOBB(va), vb);
        } else {
            return collides(va, toOBB(vb));
        }
    }, a, b);
}

AABB buildAttackAABB(mu::Vec3 pos, mu::Vec3 forward, mu::Vec3 halfExtent, float offsetFwd) {
    return AABB{
        .center = pos + forward * offsetFwd,
        .size   = halfExtent * 2.f,
    };
}

RayHit RaycastAABB(const AABB& box, const Ray& ray) {
    RayHit hit { .hit = false };

    float tmin = std::numeric_limits<float>::min();
    float tmax = std::numeric_limits<float>::max();

    mu::Vec3 normal{};

    const auto boxMin = box.center - box.size * 0.5f;
    const auto boxMax = box.center + box.size * 0.5f;

    for (int i = 0; i < 3; i++) {
        if (fabs(ray.dir[i]) < 1e-6f) {
            if (ray.origin[i] < boxMin[i] || ray.origin[i] > boxMax[i])
                return hit;
        } else {
            float invD = 1.0f / ray.dir[i];
            float t1 = (boxMin[i] - ray.origin[i]) * invD;
            float t2 = (boxMax[i] - ray.origin[i]) * invD;

            mu::Vec3 n1{};
            mu::Vec3 n2{};
            n1.setComponent(i, -1.f);
            n2.setComponent(i, 1.f);

            if (t1 > tmin) {
                tmin = t1;
                normal = n1;
            }
            if (t2 < tmax) {
                tmax = t2;
            }
            if (tmin > tmax)
                return hit;
        }
    }

    if (tmin < 0)
        return hit;

    hit.hit = true;
    hit.t = tmin;
    hit.point = ray.origin + ray.dir * tmin;
    hit.normal = normal;

    return hit;
}
