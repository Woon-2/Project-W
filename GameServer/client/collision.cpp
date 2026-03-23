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

// Returns the tightest AABB wrapping an OBB.
AABB obbToAABB(const OBB& obb) {
    const auto axes = obbLocalAxes(obb);
    const float ex = std::abs(mu::dot(axes[0], mu::Vec3(1.f,0.f,0.f))) * obb.halfExtents.x()
                   + std::abs(mu::dot(axes[1], mu::Vec3(1.f,0.f,0.f))) * obb.halfExtents.y()
                   + std::abs(mu::dot(axes[2], mu::Vec3(1.f,0.f,0.f))) * obb.halfExtents.z();
    const float ey = std::abs(mu::dot(axes[0], mu::Vec3(0.f,1.f,0.f))) * obb.halfExtents.x()
                   + std::abs(mu::dot(axes[1], mu::Vec3(0.f,1.f,0.f))) * obb.halfExtents.y()
                   + std::abs(mu::dot(axes[2], mu::Vec3(0.f,1.f,0.f))) * obb.halfExtents.z();
    const float ez = std::abs(mu::dot(axes[0], mu::Vec3(0.f,0.f,1.f))) * obb.halfExtents.x()
                   + std::abs(mu::dot(axes[1], mu::Vec3(0.f,0.f,1.f))) * obb.halfExtents.y()
                   + std::abs(mu::dot(axes[2], mu::Vec3(0.f,0.f,1.f))) * obb.halfExtents.z();
    return AABB{ obb.center, mu::Vec3(ex, ey, ez) * 2.f };
}

// --- BVH helpers ---

// Precise collision test between two variant shapes.
static CollisionResult collidesShapes(
    const std::variant<AABB, OBB>& a,
    const std::variant<AABB, OBB>& b)
{
    return std::visit([](auto&& sa, auto&& sb) -> CollisionResult {
        using A = std::decay_t<decltype(sa)>;
        using B = std::decay_t<decltype(sb)>;
        if constexpr (std::is_same_v<A, AABB> && std::is_same_v<B, AABB>)
            return collides(sa, sb);
        else if constexpr (std::is_same_v<A, OBB> && std::is_same_v<B, OBB>)
            return collides(sa, sb);
        else if constexpr (std::is_same_v<A, AABB>)
            return collides(toOBB(sa), sb);
        else
            return collides(sa, toOBB(sb));
    }, a, b);
}

// BVH vs AABB (used for combat attack hitboxes).
// Traverses the BVH with DFS. For each node whose bounds overlap the hitbox,
// runs a precise shape test. Returns on the first precise hit.
// If shape misses, still descends into children (parent shape may not tightly
// contain children — children can still overlap the hitbox).
CollisionResult collides(const BVH& bvh, const AABB& hitbox) {
    if (bvh.empty()) return CollisionResult{ .hit = false };

    std::vector<int> stack;
    stack.reserve(8);
    stack.push_back(0);

    while (!stack.empty()) {
        const int idx = stack.back(); stack.pop_back();
        const auto& node = bvh.nodes[idx];

        if (!collides(node.bounds, hitbox).hit) continue;

        const CollisionVolume dummy{};  // not used below
        const auto result = std::visit([&](auto&& s) -> CollisionResult {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, AABB>)
                return collides(s, hitbox);
            else
                return collides(s, toOBB(hitbox));
        }, node.shape);

        if (result.hit) return result;

        for (int c : node.children) stack.push_back(c);
    }

    return CollisionResult{ .hit = false };
}

// BVH vs BVH (used for physics body-body collision).
// Dual-tree DFS. Returns on the first precise shape hit.
CollisionResult collides(const BVH& a, const BVH& b) {
    if (a.empty() || b.empty()) return CollisionResult{ .hit = false };

    struct Pair { int ai, bi; };
    std::vector<Pair> stack;
    stack.reserve(16);
    stack.push_back({ 0, 0 });

    while (!stack.empty()) {
        const auto [ai, bi] = stack.back(); stack.pop_back();
        const auto& na = a.nodes[ai];
        const auto& nb = b.nodes[bi];

        if (!collides(na.bounds, nb.bounds).hit) continue;

        const auto result = collidesShapes(na.shape, nb.shape);
        if (result.hit) return result;

        const bool aLeaf = na.isLeaf();
        const bool bLeaf = nb.isLeaf();

        if (!aLeaf && !bLeaf) {
            const float volA = na.bounds.size.x() * na.bounds.size.y() * na.bounds.size.z();
            const float volB = nb.bounds.size.x() * nb.bounds.size.y() * nb.bounds.size.z();
            if (volA >= volB) {
                for (int ca : na.children) stack.push_back({ ca, bi });
            } else {
                for (int cb : nb.children) stack.push_back({ ai, cb });
            }
        } else if (!aLeaf) {
            for (int ca : na.children) stack.push_back({ ca, bi });
        } else if (!bLeaf) {
            for (int cb : nb.children) stack.push_back({ ai, cb });
        }
    }

    return CollisionResult{ .hit = false };
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
