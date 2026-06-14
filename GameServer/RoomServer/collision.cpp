#include "rspch.hpp"
#include "collision.hpp"
#include "terrain.hpp"
#include "rigidBody.hpp"
#include "contactConstraint.hpp"
#include "staticDepenetration.hpp"

CollisionResult collides(const AABB& a, const AABB& b) {
    CollisionResult res{.hit = false};

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
    CollisionResult res{.hit = false};

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

        const float rA = projectOBBOntoAxis(a, axesA, axis);
        const float rB = projectOBBOntoAxis(b, axesB, axis);
        const float dProj = mu::dot(d, axis);
        const float overlap = rA + rB - std::abs(dProj);

        if (overlap < 0.f) return res;   // separating axis found — no collision

        if (overlap < minOverlap) {
            minOverlap = overlap;
            const float sign = (dProj >= 0.f) ? -1.f : 1.f; // B→A convention
            bestAxis = axis * sign;
        }
    }

    res.hit = true;
    res.depth = minOverlap;
    res.normal = mu::NVec3(bestAxis, mu::NVec3::NoNormalize_t{});
    res.mtv = bestAxis * minOverlap;
    res.contactPoint = (a.center + b.center) * 0.5f;

    return res;
}

OBB toOBB(const AABB& aabb) {
    return OBB{
        .center = aabb.center,
        .halfExtents = aabb.size * 0.5f,
        .orient = mu::NQuat{},
    };
}

// Returns the tightest AABB wrapping an OBB.
AABB obbToAABB(const OBB& obb) {
    const auto axes = obbLocalAxes(obb);
    const float ex = std::abs(mu::dot(axes[0], mu::Vec3(1.f, 0.f, 0.f))) * obb.halfExtents.x()
        + std::abs(mu::dot(axes[1], mu::Vec3(1.f, 0.f, 0.f))) * obb.halfExtents.y()
        + std::abs(mu::dot(axes[2], mu::Vec3(1.f, 0.f, 0.f))) * obb.halfExtents.z();
    const float ey = std::abs(mu::dot(axes[0], mu::Vec3(0.f, 1.f, 0.f))) * obb.halfExtents.x()
        + std::abs(mu::dot(axes[1], mu::Vec3(0.f, 1.f, 0.f))) * obb.halfExtents.y()
        + std::abs(mu::dot(axes[2], mu::Vec3(0.f, 1.f, 0.f))) * obb.halfExtents.z();
    const float ez = std::abs(mu::dot(axes[0], mu::Vec3(0.f, 0.f, 1.f))) * obb.halfExtents.x()
        + std::abs(mu::dot(axes[1], mu::Vec3(0.f, 0.f, 1.f))) * obb.halfExtents.y()
        + std::abs(mu::dot(axes[2], mu::Vec3(0.f, 0.f, 1.f))) * obb.halfExtents.z();
    return AABB{obb.center, mu::Vec3(ex, ey, ez) * 2.f};
}

// --- Rigid world-space BVH transform (shared by Object + ScatterCollider) ---

std::variant<AABB, OBB> MU_CALLCONV transformShapeRigid(
    const std::variant<AABB, OBB>& local,
    mu::Vec3 pos, mu::NQuat orient, mu::Vec3 scale)
{
    const mu::Vec3 rx = orient.rotate(mu::Vec3(1.f, 0.f, 0.f));
    const mu::Vec3 ry = orient.rotate(mu::Vec3(0.f, 1.f, 0.f));
    const bool hasRot =
        std::abs(rx.x() - 1.f) > 1e-5f || std::abs(rx.y()) > 1e-5f || std::abs(rx.z()) > 1e-5f ||
        std::abs(ry.y() - 1.f) > 1e-5f || std::abs(ry.x()) > 1e-5f || std::abs(ry.z()) > 1e-5f;

    return std::visit([&](auto&& s) -> std::variant<AABB, OBB> {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, AABB>) {
            if (!hasRot)
                return AABB{ s.center * scale + pos, s.size * scale };
            return OBB{ orient.rotate(s.center * scale) + pos, (s.size * 0.5f) * scale, orient };
        } else {
            mu::NQuat worldOrient = orient;
            worldOrient *= s.orient;
            return OBB{ orient.rotate(s.center * scale) + pos, s.halfExtents * scale, worldOrient };
        }
    }, local);
}

BVH MU_CALLCONV makeWorldBVH(const BVH& localBVH,
                             mu::Vec3 pos, mu::NQuat orient, mu::Vec3 scale)
{
    BVH out;
    if (localBVH.empty()) return out;
    out.nodes.resize(localBVH.nodes.size());
    for (std::size_t i = 0; i < localBVH.nodes.size(); ++i) {
        const auto& src = localBVH.nodes[i];
        auto&       dst = out.nodes[i];
        dst.children    = src.children;
        dst.name        = src.name;
        dst.boneName    = src.boneName;
        dst.boneIdx     = src.boneIdx;
        dst.damageCoeff = src.damageCoeff;
        dst.shape       = transformShapeRigid(src.shape, pos, orient, scale);
        dst.bounds      = std::visit([](auto&& s) -> AABB {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, OBB>) return obbToAABB(s);
            else                                  return s;
        }, dst.shape);
    }
    return out;
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
    if (bvh.empty()) return CollisionResult{.hit = false};

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

    return CollisionResult{.hit = false};
}

// BVH vs BVH (used for physics body-body collision).
// Dual-tree DFS. Returns on the first precise shape hit.
CollisionResult collides(const BVH& a, const BVH& b) {
    if (a.empty() || b.empty()) return CollisionResult{.hit = false};

    struct Pair { int ai, bi; };
    std::vector<Pair> stack;
    stack.reserve(16);
    stack.push_back({0, 0});

    while (!stack.empty()) {
        const auto [ai, bi] = stack.back(); stack.pop_back();
        const auto& na = a.nodes[ai];
        const auto& nb = b.nodes[bi];

        if (!collides(na.bounds, nb.bounds).hit) continue;

        const bool aLeaf = na.isLeaf();
        const bool bLeaf = nb.isLeaf();

        if (aLeaf && bLeaf) {
            // Both leaf: precise shape test is definitive.
            const auto result = collidesShapes(na.shape, nb.shape);
            if (result.hit) return result;
        } else if (!aLeaf && !bLeaf) {
            // Descend into the subtree with the larger volume.
            const float volA = na.bounds.size.x() * na.bounds.size.y() * na.bounds.size.z();
            const float volB = nb.bounds.size.x() * nb.bounds.size.y() * nb.bounds.size.z();
            if (volA >= volB) {
                for (int ca : na.children) stack.push_back({ ca, bi });
            } else {
                for (int cb : nb.children) stack.push_back({ ai, cb });
            }
        } else if (!aLeaf) {
            for (int ca : na.children) stack.push_back({ ca, bi });
        } else {
            for (int cb : nb.children) stack.push_back({ ai, cb });
        }
    }

    return CollisionResult{ .hit = false };
}

OBB transformOBBByMatrix(const OBB& obb, const mu::Mat4x4& xform) {
    const mu::Vec3 newCenter(
        DirectX::XMVector3TransformCoord(obb.center.get(), xform.get()));
    const DirectX::XMVECTOR matRotQuat =
        DirectX::XMQuaternionRotationMatrix(xform.get());
    const DirectX::XMVECTOR combined =
        DirectX::XMQuaternionMultiply(obb.orient.get(), matRotQuat);
    return OBB{ newCenter, obb.halfExtents, mu::NQuat(combined, mu::NQuat::NoNormalize_t{}) };
}

BVHHitResult collides(const BVH& bvh, const OBB& hitbox) {
    BVHHitResult result{};
    if (bvh.empty()) return result;

    const AABB hitboxAABB = obbToAABB(hitbox);

    std::vector<int> stack;
    stack.reserve(8);
    stack.push_back(0);

    while (!stack.empty()) {
        const int idx = stack.back(); stack.pop_back();
        const auto& node = bvh.nodes[idx];

        if (!collides(node.bounds, hitboxAABB).hit) continue;

        if (node.isLeaf()) {
            const CollisionResult cr = std::visit([&](auto&& s) -> CollisionResult {
                using T = std::decay_t<decltype(s)>;
                if constexpr (std::is_same_v<T, OBB>)
                    return collides(s, hitbox);
                else
                    return collides(toOBB(s), hitbox);
            }, node.shape);

            if (cr.hit) {
                static_cast<CollisionResult&>(result) = cr;
                result.hitNodeIdx  = idx;
                result.damageCoeff = node.damageCoeff;
                return result;
            }
        } else {
            for (int c : node.children) stack.push_back(c);
        }
    }

    return result;
}

AABB buildAttackAABB(mu::Vec3 pos, mu::Vec3 forward, mu::Vec3 halfExtent, float offsetFwd) {
    return AABB{
        .center = pos + forward * offsetFwd,
        .size = halfExtent * 2.f,
    };
}

// ---------------------------------------------------------------------------
// TerrainCollider
// ---------------------------------------------------------------------------

TerrainCollider::TerrainCollider(RigidBody* terrainBody, const TerrainHeightField* hf)
    : terrainBody_(terrainBody), heightField_(hf)
{}

void TerrainCollider::extractBottomVertices(const BVHNode& leaf,
                                            std::vector<mu::Vec3>& out)
{
    if (std::holds_alternative<AABB>(leaf.shape)) {
        const auto& aabb = std::get<AABB>(leaf.shape);
        const mu::Vec3 c = aabb.center;
        const mu::Vec3 h = aabb.size * 0.5f;
        const float    by = c.y() - h.y();
        out.push_back(mu::Vec3(c.x() - h.x(), by, c.z() - h.z()));
        out.push_back(mu::Vec3(c.x() + h.x(), by, c.z() - h.z()));
        out.push_back(mu::Vec3(c.x() - h.x(), by, c.z() + h.z()));
        out.push_back(mu::Vec3(c.x() + h.x(), by, c.z() + h.z()));
    } else {
        const auto& obb = std::get<OBB>(leaf.shape);
        const mu::Vec3 ax = obb.orient.rotate(mu::Vec3(1.f, 0.f, 0.f));
        const mu::Vec3 ay = obb.orient.rotate(mu::Vec3(0.f, 1.f, 0.f));
        const mu::Vec3 az = obb.orient.rotate(mu::Vec3(0.f, 0.f, 1.f));
        const float    hx = obb.halfExtents.x();
        const float    hy = obb.halfExtents.y();
        const float    hz = obb.halfExtents.z();
        for (int sx : {-1, 1})
        for (int sy : {-1, 1})
        for (int sz : {-1, 1}) {
            out.push_back(obb.center
                + ax * (hx * sx)
                + ay * (hy * sy)
                + az * (hz * sz));
        }
    }
}

bool TerrainCollider::testVertex(mu::Vec3 worldVert, ContactPoint& outCp, float lookAhead) const
{
    const mu::Vec3 origin = terrainBody_->pos();

    const float localX = worldVert.x() - origin.x();
    const float localZ = worldVert.z() - origin.z();

    if (localX < 0.f || localX > heightField_->sizeX) return false;
    if (localZ < 0.f || localZ > heightField_->sizeZ) return false;

    const float terrainWorldY = origin.y() + heightField_->getHeightAt(localX, localZ);
    const float depth         = terrainWorldY - worldVert.y();
    if (depth <= -lookAhead) return false;

    outCp.worldPos = mu::Vec3(worldVert.x(), terrainWorldY, worldVert.z());
    outCp.normal   = mu::NVec3(0.f, 1.f, 0.f, mu::NVec3::NoNormalize_t{});
    outCp.depth    = std::max(0.f, depth);
    return true;
}

int TerrainCollider::collectContacts(const RigidBody& dynamic,
                                     float lookAhead,
                                     std::vector<ContactPoint>& outContacts) const
{
    if (!heightField_ || heightField_->empty()) return 0;
    if (dynamic.worldBVH().empty())             return 0;

    std::vector<mu::Vec3> verts;
    verts.reserve(32);
    for (const BVHNode& node : dynamic.worldBVH().nodes) {
        if (node.isLeaf())
            extractBottomVertices(node, verts);
    }

    std::vector<ContactPoint> candidates;
    candidates.reserve(verts.size());
    for (const mu::Vec3& v : verts) {
        ContactPoint cp;
        if (testVertex(v, cp, lookAhead))
            candidates.push_back(cp);
    }

    if (candidates.empty()) return 0;

    if (candidates.size() > 4) {
        std::partial_sort(candidates.begin(), candidates.begin() + 4, candidates.end(),
            [](const ContactPoint& a, const ContactPoint& b) {
                return a.depth > b.depth;
            });
        candidates.resize(4);
    }

    const int added = static_cast<int>(candidates.size());
    for (auto& cp : candidates)
        outContacts.push_back(cp);
    return added;
}

// ---------------------------------------------------------------------------

RayHit RaycastAABB(const AABB& box, const Ray& ray) {
    RayHit hit{.hit = false};

    float tmin = std::numeric_limits<float>::min();
    float tmax = std::numeric_limits<float>::max();

    mu::Vec3 normal{};

    const auto boxMin = box.center - box.size * 0.5f;
    const auto boxMax = box.center + box.size * 0.5f;

    for (int i = 0; i < 3; i++) {
        if (fabs(ray.dir[i]) < 1e-6f) {
            if (ray.origin[i] < boxMin[i] || ray.origin[i] > boxMax[i])
                return hit;
        }
        else {
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

// ---------------------------------------------------------------------------
// TerrainCollider (WorldCollider overrides)
// ---------------------------------------------------------------------------

bool TerrainCollider::footprintReject(mu::Vec3 bodyPos, float pad) const
{
    if (!heightField_) return true;
    const mu::Vec3 origin = terrainBody_->pos();
    if (bodyPos.x() < origin.x() - pad || bodyPos.x() > origin.x() + heightField_->sizeX + pad) return true;
    if (bodyPos.z() < origin.z() - pad || bodyPos.z() > origin.z() + heightField_->sizeZ + pad) return true;
    return false;
}

int TerrainCollider::generateContacts(RigidBody& dyn, const ContactSink& sink) const
{
    if (dyn.worldBVH().empty()) return 0;
    if (!sink.constraints)      return 0;

    const float vy        = dyn.linearVel().y();
    const float lookAhead = (vy < 0.f) ? std::min(0.15f, std::abs(vy) * sink.subDtSec) : 0.f;

    std::vector<ContactPoint> contacts;
    contacts.reserve(4);
    const int cnt = collectContacts(dyn, lookAhead, contacts);
    if (cnt == 0) return 0;

    const mu::Vec3 origin = terrainBody_->pos();
    auto cc = std::make_unique<ContactConstraint>(&dyn, terrainBody_);
    cc->setExternalAccels(sink.gravity, mu::Vec3(0.f, 0.f, 0.f));
    cc->setTerrainContact(true);
    for (auto& cp : contacts) {
        cp.localA = cp.worldPos - dyn.pos();
        cp.localB = cp.worldPos - origin;
        cc->addContact(cp);
    }
    sink.constraints->push_back(std::move(cc));
    return cnt;
}

// ---------------------------------------------------------------------------
// ScatterCollider
// ---------------------------------------------------------------------------

ScatterCollider::ScatterCollider(const std::vector<InstanceInput>& instances)
{
    insts_.reserve(instances.size());

    bool     haveBounds = false;
    mu::Vec3 lo(0.f, 0.f, 0.f), hi(0.f, 0.f, 0.f);

    for (const auto& in : instances) {
        if (!in.modelBVH || in.modelBVH->empty()) continue;

        Inst inst;
        inst.worldBVH = makeWorldBVH(*in.modelBVH, in.pos, in.rot, in.scale);
        if (inst.worldBVH.empty()) continue;

        bool     first = true;
        mu::Vec3 ilo(0.f, 0.f, 0.f), ihi(0.f, 0.f, 0.f);
        for (const auto& node : inst.worldBVH.nodes) {
            const mu::Vec3 c   = node.bounds.center;
            const mu::Vec3 h   = node.bounds.size * 0.5f;
            const mu::Vec3 nlo = c - h, nhi = c + h;
            if (first) { ilo = nlo; ihi = nhi; first = false; }
            else {
                ilo = mu::Vec3(std::min(ilo.x(), nlo.x()), std::min(ilo.y(), nlo.y()), std::min(ilo.z(), nlo.z()));
                ihi = mu::Vec3(std::max(ihi.x(), nhi.x()), std::max(ihi.y(), nhi.y()), std::max(ihi.z(), nhi.z()));
            }
        }
        inst.worldAABB = AABB{ (ilo + ihi) * 0.5f, ihi - ilo };

        if (!haveBounds) { lo = ilo; hi = ihi; haveBounds = true; }
        else {
            lo = mu::Vec3(std::min(lo.x(), ilo.x()), std::min(lo.y(), ilo.y()), std::min(lo.z(), ilo.z()));
            hi = mu::Vec3(std::max(hi.x(), ihi.x()), std::max(hi.y(), ihi.y()), std::max(hi.z(), ihi.z()));
        }
        insts_.push_back(std::move(inst));
    }

    if (insts_.empty()) return;
    bounds_ = AABB{ (lo + hi) * 0.5f, hi - lo };

    gridMinX_ = lo.x() - 0.01f;
    gridMinZ_ = lo.z() - 0.01f;
    nx_ = std::max(1, static_cast<int>((hi.x() - gridMinX_) / cellSize_) + 1);
    nz_ = std::max(1, static_cast<int>((hi.z() - gridMinZ_) / cellSize_) + 1);
    cells_.assign(static_cast<std::size_t>(nx_) * static_cast<std::size_t>(nz_), {});

    for (int i = 0; i < static_cast<int>(insts_.size()); ++i) {
        const AABB& a   = insts_[i].worldAABB;
        const float ax0 = a.center.x() - a.size.x() * 0.5f;
        const float ax1 = a.center.x() + a.size.x() * 0.5f;
        const float az0 = a.center.z() - a.size.z() * 0.5f;
        const float az1 = a.center.z() + a.size.z() * 0.5f;
        const int cx0 = std::clamp(static_cast<int>((ax0 - gridMinX_) / cellSize_), 0, nx_ - 1);
        const int cx1 = std::clamp(static_cast<int>((ax1 - gridMinX_) / cellSize_), 0, nx_ - 1);
        const int cz0 = std::clamp(static_cast<int>((az0 - gridMinZ_) / cellSize_), 0, nz_ - 1);
        const int cz1 = std::clamp(static_cast<int>((az1 - gridMinZ_) / cellSize_), 0, nz_ - 1);
        for (int cz = cz0; cz <= cz1; ++cz)
            for (int cx = cx0; cx <= cx1; ++cx)
                cells_[cellIndex(cx, cz)].push_back(i);
    }
    visited_.assign(insts_.size(), -1);
}

bool ScatterCollider::footprintReject(mu::Vec3 bodyPos, float pad) const
{
    if (insts_.empty()) return true;
    const mu::Vec3 c = bounds_.center;
    const mu::Vec3 h = bounds_.size * 0.5f;
    if (std::abs(bodyPos.x() - c.x()) > h.x() + pad) return true;
    if (std::abs(bodyPos.z() - c.z()) > h.z() + pad) return true;
    return false;
}

int ScatterCollider::generateContacts(RigidBody& dyn, const ContactSink& sink) const
{
    const BVH& bvh = dyn.worldBVH();
    if (bvh.empty() || insts_.empty() || !sink.staticContacts) return 0;

    const AABB bodyAABB = bvh.nodes[0].bounds;   // root subtree bounds

    int added = 0;
    forEachCandidate(bodyAABB, [&](int i) {
        const Inst& inst = insts_[i];
        if (!collides(bodyAABB, inst.worldAABB).hit) return;

        const CollisionResult res = collides(bvh, inst.worldBVH);
        if (!res.hit) return;

        // res.normal is B->A = static(inst) -> movable(dyn): the push-out direction.
        mu::NVec3 n = res.normal;
        if (mu::Vec3(n).len2() < 0.5f) {
            const mu::Vec3 sep = dyn.pos() - inst.worldAABB.center;
            n = (sep.len2() > 1e-6f)
                ? mu::normalize(sep)
                : mu::NVec3(0.f, 1.f, 0.f, mu::NVec3::NoNormalize_t{});
        }

        StaticContact sc;
        sc.movable    = &dyn;
        sc.staticBody = nullptr;
        ContactPoint cp;
        cp.worldPos = res.contactPoint;
        cp.normal   = n;
        cp.depth    = res.depth;
        cp.localA   = res.contactPoint - dyn.pos();
        sc.addContact(cp);
        sink.staticContacts->push_back(std::move(sc));
        ++added;
    });
    return added;
}
