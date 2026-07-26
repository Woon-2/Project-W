#include "pch.hpp"
#include "ragdoll.hpp"
#include "jointConstraint.hpp"
#include "physicsWorld.hpp"
#include "mesh.hpp"

// ---------------------------------------------------------------------------
// Internal file-scope helpers
// ---------------------------------------------------------------------------

static const Bone* findBoneByName(const Skeleton& skel, std::string_view name)
{
    if (!skel.bones) return nullptr;
    for (const Bone& b : *skel.bones)
        if (b.name == name) return &b;
    return nullptr;
}

// Extract translation from a 4x4 matrix (row-vector convention: row 3 = translation).
static mu::Vec3 extractPos(mu::Mat4x4 m)
{
    auto r = m.row(3);
    return mu::Vec3(r.x(), r.y(), r.z());
}

// Extract normalised quaternion from a rotation (+ translation) matrix.
// Strips (uniform) scale from the basis first: inputs like bone.toDress * finalXform *
// objectWorldMat carry the object scale, and quatRotMat on a scaled basis yields a distorted
// quaternion. Pure-rotation inputs are unaffected (rows already unit length).
static mu::NQuat extractOrient(mu::Mat4x4 m)
{
    for (int i = 0; i < 3; ++i) {
        const mu::Vec4 r = m.row(i);
        mu::Vec3 axis(r.x(), r.y(), r.z());
        if (axis.len2() > 1e-12f) axis = mu::Vec3(mu::normalize(axis));
        m.setRow(i, mu::Vec4(axis.x(), axis.y(), axis.z(), r.w()));
    }
    return mu::NQuat(mu::quatRotMat(m.get()));
}

// Construct a 4x4 rigid-body world matrix from position and orientation.
// The rotation occupies the upper 3x3 and the position occupies row 3.
static mu::Mat4x4 makeRigidMat(mu::Vec3 pos, mu::NQuat orient)
{
    mu::Mat4x4 m(orient);
    m.setRow(3, mu::Vec4(pos.x(), pos.y(), pos.z(), 1.f));
    return m;
}

static mu::Vec3 safeNormalizeOr(mu::Vec3 v, mu::Vec3 fallback)
{
    return (v.len2() > 1e-8f) ? mu::Vec3(mu::normalize(v)) : fallback;
}

// Collision-filter constants for ragdoll bones.
// kRagdollMask allows ragdoll-ragdoll contacts; per-pair filtering via
// PhysicsWorld::ignoreCollisionPairs_ (registered in activate()) handles
// joint-connected and near-chain pair suppression.
static constexpr uint16_t kRagdollGroup = 2u;
static constexpr uint16_t kRagdollMask  = 0xFFFFu;

// Build (or rebuild) the world-space single-OBB BVH for a ragdoll bone body.
// body->pos() == OBB world centre (capsuleOffset is already baked into the body position).
// Called once before activate() to seed the BVH, then every physics step via the
// registered onRebuildBVH callback so TerrainCollider can generate contacts.
static void rebuildBoneBodyBVH(RigidBody* body, mu::Vec3 halfExtents)
{
    OBB obb;
    obb.center      = body->pos();
    obb.halfExtents = halfExtents;
    obb.orient      = body->orient();

    // Enclosing AABB via rotation-matrix projection (R = local-to-world, row-vector convention):
    //   world_half_i = sum_j |R[j][i]| * half_j
    const mu::Mat4x4 rotMat(body->orient());
    const auto r0 = rotMat.row(0);  // local X axis in world
    const auto r1 = rotMat.row(1);  // local Y axis in world
    const auto r2 = rotMat.row(2);  // local Z axis in world
    const float wx = std::abs(r0.x()) * halfExtents.x()
                   + std::abs(r1.x()) * halfExtents.y()
                   + std::abs(r2.x()) * halfExtents.z();
    const float wy = std::abs(r0.y()) * halfExtents.x()
                   + std::abs(r1.y()) * halfExtents.y()
                   + std::abs(r2.y()) * halfExtents.z();
    const float wz = std::abs(r0.z()) * halfExtents.x()
                   + std::abs(r1.z()) * halfExtents.y()
                   + std::abs(r2.z()) * halfExtents.z();

    AABB bounds;
    bounds.center = obb.center;
    bounds.size   = mu::Vec3(wx * 2.f, wy * 2.f, wz * 2.f);

    BVH& bvh = body->worldBVH();
    if (bvh.nodes.empty()) bvh.nodes.resize(1);
    bvh.nodes[0].shape    = obb;
    bvh.nodes[0].bounds   = bounds;
    bvh.nodes[0].children.clear();
    bvh.nodes[0].boneIdx  = -1;
}

// ---------------------------------------------------------------------------
// Ragdoll::findBone
// ---------------------------------------------------------------------------

const RagdollBone* Ragdoll::findBone(int boneIdx) const
{
    for (const RagdollBone& rb : bones_)
        if (rb.boneIdx == boneIdx) return &rb;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Ragdoll::build
// ---------------------------------------------------------------------------

void Ragdoll::build(const Skeleton& skel, const RagdollDef& def, PhysicsWorld& world, mu::Vec3 modelScale)
{
    bones_.clear(); bodies_.clear(); joints_.clear();
    jointBodies_.clear(); ignoredPairs_.clear();
    modelScale_ = modelScale;

    bones_.reserve(def.bones.size());
    bodies_.reserve(def.bones.size());

    // ------------------------------------------------------------------
    // Pass 1: create one RigidBody per BoneBoxDef.
    // Seed T-pose orientation so HingeJoint can read a valid orient when
    // joints are built in Pass 2.
    // ------------------------------------------------------------------
    for (const BoneBoxDef& bd : def.bones) {
        const Bone* bone = findBoneByName(skel, bd.boneName);
        if (!bone) continue;

        // Apply model x instance scale to box dimensions and the capsule offset so the bodies
        // sit on the scaled skeleton (bone origin is scaled via objectWorldMat).
        const mu::Vec3 scaledHalf   = bd.halfExtents * modelScale;
        const mu::Vec3 scaledCenter = bd.center * modelScale;
        auto body = std::make_unique<RigidBody>(MotionType::Kinematic);
        body->setMass(bd.mass);

        // [SAFETY 4] Inertia floor (see client/docs/ragdollSafety.md): a box with two
        // ~zero half-extents has a zero principal moment -> setInertia inverts to Inf
        // invInertia -> 0*Inf=NaN in the angular impulse, which the joint graph spreads
        // through the whole ragdoll in one solve sweep. Floor each inertia half-extent to
        // a fraction of the largest. Only the INERTIA box is floored; rb.halfExtents
        // (collision shape, set below) keep the authored dimensions.
        mu::Vec3 inertiaHalf = scaledHalf;
        {
            const float maxH   = std::max({ inertiaHalf.x(), inertiaHalf.y(), inertiaHalf.z() });
            const float floorH = maxH * 0.05f;
            auto cl = [&](float h) { return h < floorH ? floorH : h; };
            inertiaHalf = mu::Vec3(cl(inertiaHalf.x()), cl(inertiaHalf.y()), cl(inertiaHalf.z()));
        }
        body->setInertia(computeBoxInertia(bd.mass, inertiaHalf));
        body->setLinearDamping(bd.linearDamping);
        body->setAngularDamping(bd.angularDamping);
        body->setFriction(bd.friction);
        body->setRestitution(bd.restitution);

        // Set T-pose orientation (no position yet; syncFromPose seeds it later).
        body->setOrient(extractOrient(bone->toDress));
        body->snapToCurrent();

        RagdollBone rb;
        rb.boneIdx       = bone->boneIdx;
        rb.body          = body.get();
        rb.capsuleOffset = scaledCenter;    // scaled: rotated by pure orientation in seed/sync
        rb.halfExtents   = scaledHalf;
        rb.noiseImpulse  = bd.noiseImpulse;

        bones_.push_back(rb);
        bodies_.push_back(std::move(body));
    }

    // ------------------------------------------------------------------
    // Pass 2: create one Constraint per JointDef.
    // Anchors are computed from the T-pose skeleton data so they are
    // valid even before syncFromPose has been called.
    // ------------------------------------------------------------------
    joints_.reserve(def.joints.size());

    for (const JointDef& jd : def.joints) {
        const Bone* parentBone = findBoneByName(skel, jd.parentBoneName);
        const Bone* childBone  = findBoneByName(skel, jd.childBoneName);
        if (!parentBone || !childBone) continue;

        const RagdollBone* parentRB = findBone(parentBone->boneIdx);
        const RagdollBone* childRB  = findBone(childBone->boneIdx);
        if (!parentRB || !childRB) continue;

        // T-pose orientation and origin for the parent bone.
        const mu::NQuat parentOrient = extractOrient(parentBone->toDress);
        const mu::Vec3  parentOrigin = extractPos(parentBone->toDress);

        // Parent body centre in dress space.
        const mu::Vec3 parentBodyCenter =
            parentOrigin + parentOrient.rotate(parentRB->capsuleOffset);

        // Child bone origin in dress space (= joint pivot world position).
        const mu::Vec3 childOrigin = extractPos(childBone->toDress);

        // anchorA: joint pivot in parent body local space.
        const mu::Vec3 anchorA =
            (~parentOrient).rotate(childOrigin - parentBodyCenter);

        // anchorB: child bone origin relative to child body centre, in child body
        // local space.  Body centre = bone origin + capsuleOffset, so bone origin
        // is at -capsuleOffset in body local space.
        const mu::Vec3 anchorB = -childRB->capsuleOffset;

        RigidBody* bodyA = parentRB->body;
        RigidBody* bodyB = childRB->body;

        std::unique_ptr<Constraint> joint;
        switch (jd.type) {
        case JointType::BallSocket:
            joint = std::make_unique<BallSocketJoint>(bodyA, bodyB, anchorA, anchorB);
            break;

        case JointType::Hinge:
            // HingeJoint internally stores refOrient = conj(bodyA.orient)*bodyB.orient
            // at construction time.  Bodies carry T-pose orients from Pass 1, so the
            // reference (neutral angle = 0) is the T-pose.
            joint = std::make_unique<HingeJoint>(
                bodyA, bodyB, anchorA, anchorB,
                jd.axisLocalA,
                jd.minAngle, jd.maxAngle);
            break;

        case JointType::ConeTwist:
        {
            const mu::Vec3 childOriginDress = extractPos(childBone->toDress);
            const mu::Vec3 parentOriginDress = extractPos(parentBone->toDress);
            const mu::Vec3 axisWorldDress = safeNormalizeOr(
                childOriginDress - parentOriginDress,
                parentOrient.rotate(mu::Vec3{ 0.f, 0.f, 1.f }));
            const mu::Vec3 axisLocalA = safeNormalizeOr(
                (~parentOrient).rotate(axisWorldDress),
                mu::Vec3{ 0.f, 0.f, 1.f });
            joint = std::make_unique<ConeTwistJoint>(
                bodyA, bodyB, anchorA, anchorB,
                extractOrient(parentBone->toDress),
                extractOrient(childBone->toDress),
                axisLocalA,
                jd.coneHalfAngle, jd.twistLimit);
            break;
        }
        }

        if (!joint) continue;

        // Parallel record before ownership is moved.
        jointBodies_.push_back({ bodyA, bodyB });

        // Tag the child RagdollBone with its parent joint (non-owning view).
        for (RagdollBone& rb : bones_) {
            if (rb.boneIdx == childBone->boneIdx) {
                rb.parentJoint = joint.get();
                break;
            }
        }

        joints_.push_back(std::move(joint));
    }
}

// ---------------------------------------------------------------------------
// Ragdoll::destroy
// ---------------------------------------------------------------------------

void Ragdoll::destroy(PhysicsWorld& world)
{
    // Unregister only if active (i.e., activate() was called and registered them).
    // If the ragdoll was never activated, bodies/joints were never registered.
    if (active_) {
        // Joints first: they hold raw RigidBody* and must not outlive the bodies.
        for (auto& j : joints_)
            world.removeJointRef(j.get());
        for (auto& b : bodies_)
            world.unregisterBody(b.get());
        for (const auto& [a, b] : ignoredPairs_)
            world.setIgnoreCollision(a, b, false);
        ignoredPairs_.clear();
    }

    joints_.clear();
    jointBodies_.clear();
    bodies_.clear();
    bones_.clear();
    passengers_.clear();
    active_ = false;
}

// ---------------------------------------------------------------------------
// Ragdoll::seedFromFinalXforms
//
// Converts AnimBlender::finalXformData() entries back to world-space body
// transforms.  Each entry is (bone.toLocal * boneXformDress), so:
//   boneWorldMat = bone.toDress * finalXforms[i] * objectWorldMat
// ---------------------------------------------------------------------------

void Ragdoll::seedFromFinalXforms(const std::vector<mu::Mat4x4>& finalXforms,
                                   const Skeleton& skel,
                                   mu::Mat4x4 objectWorldMat)
{
    if (!skel.bones) return;
    for (RagdollBone& rb : bones_) {
        if (rb.boneIdx < 0 || rb.boneIdx >= static_cast<int>(skel.bones->size())) continue;
        const Bone& bone = (*skel.bones)[rb.boneIdx];
        if (rb.boneIdx >= static_cast<int>(finalXforms.size())) continue;

        const mu::Mat4x4 boneWorldMat = bone.toDress * finalXforms[rb.boneIdx] * objectWorldMat;
        const mu::NQuat  boneOrient   = extractOrient(boneWorldMat);
        const mu::Vec3   boneOrigin   = extractPos(boneWorldMat);

        rb.body->setPos(boneOrigin + boneOrient.rotate(rb.capsuleOffset));
        rb.body->setOrient(boneOrient);
        rb.body->snapToCurrent();
    }
}

// ---------------------------------------------------------------------------
// Ragdoll::syncToFinalXforms
//
// Inverse of seedFromFinalXforms.
//   seedFromFinalXforms: boneWorldMat = bone.toDress * finalXforms[i] * objectWorldMat
//   syncToFinalXforms:  finalXforms[i] = bone.toLocal * (boneWorldMat / objectWorldMat)
// ---------------------------------------------------------------------------

void Ragdoll::syncToFinalXforms(std::vector<mu::Mat4x4>& finalXforms,
                                 const Skeleton& skel,
                                 mu::Mat4x4 objectWorldMat,
                                 float tPhysic) const
{
    if (!skel.bones) return;
    // The caller's accumulator ratio can exceed 1 when the physics loop hits its
    // per-frame step cap; clamp so slerp/lerp never extrapolate.
    const float t = std::clamp(tPhysic, 0.f, 1.f);

    for (const RagdollBone& rb : bones_) {
        if (rb.boneIdx < 0 || rb.boneIdx >= static_cast<int>(skel.bones->size())) continue;
        if (rb.boneIdx >= static_cast<int>(finalXforms.size())) continue;
        const Bone& bone = (*skel.bones)[rb.boneIdx];

        const mu::Vec3  bodyPos    = PhysicsWorld::interpolatePos(*rb.body, t);
        const mu::NQuat bodyOrient = PhysicsWorld::interpolateOrient(*rb.body, t);

        const mu::Vec3 boneOriginWorld =
            mu::Vec3(bodyPos - bodyOrient.rotate(rb.capsuleOffset));
        // Inject model scale so the skinned mesh stays scaled: without it boneWorldMat is rigid
        // and objectWorldMat's scale cancels out in finalXform * world, snapping the mesh back to
        // its unscaled size on ragdoll activation.
        const mu::Mat4x4 boneWorldMat =
            mu::Mat4x4(mu::scale(modelScale_)) * makeRigidMat(boneOriginWorld, bodyOrient);

        finalXforms[rb.boneIdx] = bone.toLocal * (boneWorldMat / objectWorldMat);
    }

    for (const PassengerBone& pb : passengers_) {
        if (pb.boneIdx         >= static_cast<int>(finalXforms.size()) ||
            pb.ancestorBoneIdx >= static_cast<int>(finalXforms.size())) continue;
        finalXforms[pb.boneIdx] = pb.relativeXform * finalXforms[pb.ancestorBoneIdx];
    }
}

// ---------------------------------------------------------------------------
// Ragdoll::activate / deactivate
// ---------------------------------------------------------------------------

void Ragdoll::activate(PhysicsWorld& world)
{
    if (active_) return;

    // Recompute joint anchors (anchorA_) from current seeded body positions.
    // build() computed them from T-pose; without this reset the joints would have
    // a large initial position error equal to the T-pose vs animation-pose offset,
    // producing huge Baumgarte correction impulses that cause the ragdoll to spin.
    for (auto& joint : joints_)
        joint->resetAnchors();

    // Build initial BVH from seeded positions, then register with a rebuild callback
    // so TerrainCollider can generate contacts every physics step.
    for (const RagdollBone& rb : bones_) {
        rebuildBoneBodyBVH(rb.body, rb.halfExtents);
        world.registerBody(rb.body,
            [body = rb.body, h = rb.halfExtents]() { rebuildBoneBodyBVH(body, h); },
            kRagdollGroup, kRagdollMask);
    }
    for (auto& joint : joints_)
        world.addJointRef(joint.get());

    // Register per-pair collision ignores.
    // 1-hop: directly jointed body pairs.
    // 2-hop: bodies sharing a common joint-neighbor (sibling pairs in chain).
    {
        ignoredPairs_.clear();

        auto normPair = [](RigidBody* a, RigidBody* b) {
            return a < b ? std::make_pair(a, b) : std::make_pair(b, a);
        };
        std::set<std::pair<RigidBody*, RigidBody*>> pending;

        // 1-hop: each joint's body pair.
        for (const auto& [bA, bB] : jointBodies_)
            pending.insert(normPair(bA, bB));

        // Build adjacency map for 2-hop computation.
        std::unordered_map<RigidBody*, std::vector<RigidBody*>> adj;
        for (const auto& [bA, bB] : jointBodies_) {
            adj[bA].push_back(bB);
            adj[bB].push_back(bA);
        }

        // 2-hop: every pair of neighbors sharing the same intermediate body.
        for (auto& [mid, neighbors] : adj) {
            const int n = static_cast<int>(neighbors.size());
            for (int i = 0; i < n; ++i)
                for (int j = i + 1; j < n; ++j)
                    pending.insert(normPair(neighbors[i], neighbors[j]));
        }

        // [SAFETY 2] Auto-ignore pairs whose collision boxes already overlap at the
        // seeded pose (see client/docs/ragdollSafety.md). Ragdoll defs commonly have
        // boxes that interpenetrate at rest (clavicle vs chest/neck, etc.); left
        // colliding they fire large contact impulses the instant the bodies turn
        // Dynamic -- a ragdoll-explosion source that scales with the model. 1/2-hop
        // neighbours are already covered above.
        {
            auto boneOBB = [](const RagdollBone& rb) {
                OBB o;
                o.center      = rb.body->pos();
                o.halfExtents = rb.halfExtents;
                o.orient      = rb.body->orient();
                return o;
            };
            for (size_t i = 0; i < bones_.size(); ++i) {
                if (!bones_[i].body) continue;
                for (size_t j = i + 1; j < bones_.size(); ++j) {
                    if (!bones_[j].body) continue;
                    const auto key = normPair(bones_[i].body, bones_[j].body);
                    if (pending.count(key)) continue;
                    if (collides(boneOBB(bones_[i]), boneOBB(bones_[j])).hit)
                        pending.insert(key);
                }
            }
        }

        for (const auto& p : pending) {
            ignoredPairs_.push_back(p);
            world.setIgnoreCollision(p.first, p.second, true);
        }
    }

    for (auto& body : bodies_)
        body->setMotionType(MotionType::Dynamic);

    // Ragdoll-only gravity. Applied here rather than in build() because it is only
    // meaningful once the bodies are Dynamic (Kinematic bodies ignore gravity).
    for (auto& body : bodies_)
        body->setGravityScale(kGravityScale);

    active_ = true;
}

// ---------------------------------------------------------------------------
// Ragdoll::applyDeathKick
// ---------------------------------------------------------------------------

void Ragdoll::applyDeathKick(mu::Vec3 initVel)
{
    if (bones_.empty() || !bones_[0].body) return;

    // Toppling axis: horizontal, perpendicular to the knock-back direction, so the
    // body falls AWAY from whatever hit it. With no knock-back, pick a random
    // horizontal axis so repeated kills do not all collapse identically.
    const bool     hasVel = initVel.len2() > 0.01f;
    const mu::Vec3 velDir = hasVel ? mu::Vec3(mu::normalize(initVel)) : mu::Vec3{};
    mu::Vec3 axis = hasVel ? mu::Vec3(mu::cross(mu::Vec3(0.f, 1.f, 0.f), velDir))
                           : mu::Vec3(rand(-1.f, 1.f), 0.f, rand(-1.f, 1.f));
    axis = safeNormalizeOr(axis, mu::Vec3(1.f, 0.f, 0.f));

    // Pivot: the ground contact under the root body. Rotating about the feet is what
    // makes the character topple instead of spinning about its waist.
    mu::Vec3 pivot = bones_[0].body->pos();
    for (const RagdollBone& rb : bones_)
        if (rb.body) pivot = mu::Vec3(pivot.x(), std::min(pivot.y(), rb.body->pos().y()), pivot.z());

    const mu::Vec3 omega = mu::Vec3(axis * kTopplingOmega);

    for (const RagdollBone& rb : bones_) {
        if (!rb.body) continue;
        // One rigid velocity field over the whole assembly: every joint sees zero
        // relative velocity, so this cannot blow the constraint chain apart.
        const mu::Vec3 r = mu::Vec3(rb.body->pos() - pivot);
        rb.body->setLinearVel(mu::Vec3(initVel + mu::cross(omega, r)));
        rb.body->setOmega(omega);
    }

    // Per-bone noise: direction biased toward the death velocity, applied at a random
    // point INSIDE the bone box rather than at its centre of mass, so it contributes a
    // little spin. (Applying it at body->pos() produces pure translation and no torque.)
    constexpr float kNoiseBias = 0.6f;
    for (const RagdollBone& rb : bones_) {
        if (rb.noiseImpulse <= 0.f || !rb.body) continue;

        const mu::Vec3 rnd = safeNormalizeOr(
            mu::Vec3(rand(-1.f, 1.f), rand(-1.f, 1.f), rand(-1.f, 1.f)),
            mu::Vec3(0.f, 0.f, 1.f));
        const mu::Vec3 dir = safeNormalizeOr(
            mu::Vec3(velDir * kNoiseBias + rnd * (1.f - kNoiseBias)),
            mu::Vec3(0.f, 0.f, 1.f));

        const mu::Vec3 localOff(rand(-1.f, 1.f) * rb.halfExtents.x(),
                                rand(-1.f, 1.f) * rb.halfExtents.y(),
                                rand(-1.f, 1.f) * rb.halfExtents.z());
        rb.body->applyImpulse(mu::Vec3(dir * rb.noiseImpulse),
                              mu::Vec3(rb.body->pos() + rb.body->orient().rotate(localOff)));
    }
}

void Ragdoll::deactivate(PhysicsWorld& world)
{
    for (auto& body : bodies_)
        body->setMotionType(MotionType::Kinematic);

    // Joints first: they hold raw RigidBody* and must not outlive the bodies.
    for (auto& joint : joints_)
        world.removeJointRef(joint.get());
    for (auto& body : bodies_)
        world.unregisterBody(body.get());

    for (const auto& [a, b] : ignoredPairs_)
        world.setIgnoreCollision(a, b, false);
    ignoredPairs_.clear();

    passengers_.clear();
    active_ = false;
}

// ---------------------------------------------------------------------------
// Ragdoll::buildPassengers
//
// Bind every non-ragdoll bone rigidly to a ragdoll body so the whole skeleton
// follows the physics simulation.  A bone left unbound keeps its death-pose
// finalXforms; since the owning Object's world matrix is also frozen while the
// ragdoll is active, such a bone stays fixed in world space — anchoring the mesh
// at a fixed point and stretching it when the rest of the ragdoll moves.
//
// Two passes:
//   1) buildPassengersDFS — bones that DESCEND from a ragdoll bone bind to their
//      nearest ragdoll ancestor (preserves intra-limb rigidity).
//   2) Orphan pass — bones with NO ragdoll ancestor (above/beside the bodies in
//      the hierarchy, e.g. a central hub bone) bind to the nearest ragdoll body
//      found by an undirected skeleton walk (parent + children).
//
// Row-vector convention: passenger = relativeXform * ancestor
// => relativeXform = passenger / ancestor = passenger * inv(ancestor)
// ---------------------------------------------------------------------------

void Ragdoll::buildPassengers(const Skeleton& skel,
                               const std::vector<mu::Mat4x4>& finalXforms)
{
    passengers_.clear();
    if (!skel.pRoot || !skel.bones) return;

    // Pass 1: ancestor-based binding for descendants of ragdoll bodies.
    buildPassengersDFS(skel.pRoot, -1, finalXforms);

    // Pass 2: attach orphan bones (no ragdoll ancestor) to their nearest body.
    const std::vector<Bone>& bones = *skel.bones;
    const int n = static_cast<int>(bones.size());
    if (n == 0) return;

    // Child -> parent map (Bone stores only its children).
    std::vector<int> parent(n, -1);
    for (const Bone& b : bones)
        for (const Bone* c : b.children)
            if (c->boneIdx >= 0 && c->boneIdx < n)
                parent[c->boneIdx] = b.boneIdx;

    // Multi-source BFS from every ragdoll body over the undirected skeleton graph
    // to find each bone's nearest ragdoll body (by hop count).
    std::vector<int> nearestBody(n, -1);
    std::vector<int> queue;
    queue.reserve(n);
    for (const RagdollBone& rb : bones_) {
        if (rb.boneIdx >= 0 && rb.boneIdx < n && nearestBody[rb.boneIdx] < 0) {
            nearestBody[rb.boneIdx] = rb.boneIdx;
            queue.push_back(rb.boneIdx);
        }
    }
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const int cur = queue[head];
        const int src = nearestBody[cur];
        auto visit = [&](int nb) {
            if (nb >= 0 && nb < n && nearestBody[nb] < 0) {
                nearestBody[nb] = src;
                queue.push_back(nb);
            }
        };
        visit(parent[cur]);
        for (const Bone* c : bones[cur].children)
            visit(c->boneIdx);
    }

    // Mark bones already driven (ragdoll bodies + Pass-1 passengers).
    std::vector<bool> bound(n, false);
    for (const RagdollBone& rb : bones_)
        if (rb.boneIdx >= 0 && rb.boneIdx < n) bound[rb.boneIdx] = true;
    for (const PassengerBone& pb : passengers_)
        if (pb.boneIdx >= 0 && pb.boneIdx < n) bound[pb.boneIdx] = true;

    // Bind each remaining orphan bone to its nearest ragdoll body.
    const int fx = static_cast<int>(finalXforms.size());
    for (int i = 0; i < n; ++i) {
        if (bound[i]) continue;
        const int anc = nearestBody[i];
        if (anc < 0) continue;               // no ragdoll bodies at all
        if (i >= fx || anc >= fx) continue;  // finalXforms bounds guard
        PassengerBone pb;
        pb.boneIdx         = i;
        pb.ancestorBoneIdx = anc;
        pb.relativeXform   = finalXforms[i] / finalXforms[anc];
        passengers_.push_back(pb);
    }
}

void Ragdoll::buildPassengersDFS(const Bone* bone, int currentAncestorIdx,
                                  const std::vector<mu::Mat4x4>& finalXforms)
{
    const RagdollBone* rdBone = findBone(bone->boneIdx);

    if (rdBone) {
        currentAncestorIdx = bone->boneIdx;
    } else if (currentAncestorIdx >= 0 &&
               bone->boneIdx         < static_cast<int>(finalXforms.size()) &&
               currentAncestorIdx    < static_cast<int>(finalXforms.size())) {
        PassengerBone pb;
        pb.boneIdx         = bone->boneIdx;
        pb.ancestorBoneIdx = currentAncestorIdx;
        pb.relativeXform   = finalXforms[bone->boneIdx] / finalXforms[currentAncestorIdx];
        passengers_.push_back(pb);
    }

    for (const Bone* child : bone->children)
        buildPassengersDFS(child, currentAncestorIdx, finalXforms);
}

// ---------------------------------------------------------------------------
// Ragdoll::syncFromPose  (public entry)
// ---------------------------------------------------------------------------

void Ragdoll::syncFromPose(const std::vector<AnimFrame>& pose,
                            const Skeleton& skel,
                            mu::Mat4x4 objectWorldMat)
{
    if (!skel.pRoot) return;
    syncFromPoseDFS(skel.pRoot, objectWorldMat, pose);
}

// ---------------------------------------------------------------------------
// Ragdoll::syncFromPoseDFS
//
// Row-vector convention: boneWorld = localAnimMat * parentWorld
// (mirrors AnimBlender::traverseBone which does xform *= parentXform).
// ---------------------------------------------------------------------------

void Ragdoll::syncFromPoseDFS(const Bone* bone,
                               mu::Mat4x4 parentWorldMat,
                               const std::vector<AnimFrame>& pose) const
{
    // Compute this bone's world matrix.
    const mu::Mat4x4 boneWorldMat =
        convertAnimFrameToMatrix(pose[bone->boneIdx]) * parentWorldMat;

    // Update the matching rigid body if one exists.
    const RagdollBone* rb = findBone(bone->boneIdx);
    if (rb) {
        // Body centre = bone origin + capsuleOffset rotated into world. extractOrient strips
        // scale and capsuleOffset is already scaled, so rotate it by the pure orientation
        // (matches seedFromFinalXforms) instead of the scaled boneWorldMat (avoids double scale).
        const mu::NQuat boneOrient = extractOrient(boneWorldMat);
        const mu::Vec3  boneOrigin = extractPos(boneWorldMat);

        rb->body->setPos(boneOrigin + boneOrient.rotate(rb->capsuleOffset));
        rb->body->setOrient(boneOrient);

        if (!active_) {
            // Kinematic: snap prev = curr to avoid one-frame interpolation artefact.
            rb->body->snapToCurrent();
        }
    }

    for (const Bone* child : bone->children)
        syncFromPoseDFS(child, boneWorldMat, pose);
}

// ---------------------------------------------------------------------------
// Ragdoll::syncToPose  (public entry)
// ---------------------------------------------------------------------------

void Ragdoll::syncToPose(std::vector<AnimFrame>& outPose,
                          const Skeleton& skel,
                          mu::Mat4x4 objectWorldMat) const
{
    if (!skel.pRoot) return;
    syncToPoseDFS(skel.pRoot, objectWorldMat, outPose);
}

// ---------------------------------------------------------------------------
// Ragdoll::syncToPoseDFS
//
// For each ragdoll bone: reconstruct bone-origin world matrix from the body,
// then compute the parent-relative AnimFrame and write it to outPose.
//
// For bones not covered by the ragdoll: compute their world matrix from the
// existing outPose data (animation) so DFS can continue to their children.
//
// parentWorldMat is the bone-origin world matrix of the parent bone (not the
// capsule-centre world matrix).  This keeps the hierarchy consistent with
// syncFromPoseDFS which also uses bone-origin world matrices.
// ---------------------------------------------------------------------------

void Ragdoll::syncToPoseDFS(const Bone* bone,
                             mu::Mat4x4 parentWorldMat,
                             std::vector<AnimFrame>& outPose) const
{
    mu::Mat4x4 boneWorldMat;

    const RagdollBone* rb = findBone(bone->boneIdx);
    if (rb) {
        // Reconstruct bone origin in world space.
        // body->pos() == boneOriginWorld + body->orient().rotate(capsuleOffset)
        const mu::Vec3 boneOriginWorld =
            rb->body->pos() - rb->body->orient().rotate(rb->capsuleOffset);

        boneWorldMat = makeRigidMat(boneOriginWorld, rb->body->orient());

        // Compute parent-relative transform.
        // boneWorldMat = localMat * parentWorldMat
        // => localMat  = boneWorldMat * inv(parentWorldMat) = boneWorldMat / parentWorldMat
        const mu::Mat4x4 localMat = boneWorldMat / parentWorldMat;

        auto tr = localMat.row(3);
        outPose[bone->boneIdx].translation = mu::Vec3(tr.x(), tr.y(), tr.z());
        outPose[bone->boneIdx].rotation    = extractOrient(localMat);
        outPose[bone->boneIdx].scale       = mu::Vec3(1.f, 1.f, 1.f);
    }
    else {
        // Not a ragdoll bone: derive world matrix from existing outPose data
        // so children can correctly compute their parent-relative transforms.
        boneWorldMat =
            convertAnimFrameToMatrix(outPose[bone->boneIdx]) * parentWorldMat;
    }

    for (const Bone* child : bone->children)
        syncToPoseDFS(child, boneWorldMat, outPose);
}
