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

// Extract normalised quaternion from a pure rotation (+ translation) matrix.
static mu::NQuat extractOrient(mu::Mat4x4 m)
{
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

// Collision-filter constants for ragdoll bones.
// Group 2 clears bit 1 from its own mask so ragdoll bones skip each other.
static constexpr uint16_t kRagdollGroup = 2u;
static constexpr uint16_t kRagdollMask  = static_cast<uint16_t>(0xFFFFu & ~uint16_t(kRagdollGroup));

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

void Ragdoll::build(const Skeleton& skel, const RagdollDef& def, PhysicsWorld& world)
{
    bones_.reserve(def.bones.size());
    bodies_.reserve(def.bones.size());

    // ------------------------------------------------------------------
    // Pass 1: create one RigidBody per BoneCapsuleDef.
    // Seed T-pose orientation so HingeJoint can read a valid orient when
    // joints are built in Pass 2.
    // ------------------------------------------------------------------
    for (const BoneCapsuleDef& bd : def.bones) {
        const Bone* bone = findBoneByName(skel, bd.boneName);
        if (!bone) continue;

        auto body = std::make_unique<RigidBody>(MotionType::Kinematic);
        body->setMass(bd.mass);
        body->setInertia(computeCapsuleInertia(bd.mass, bd.radius, bd.halfHeight));
        body->setLinearDamping(0.1f);
        body->setAngularDamping(0.2f);
        body->setFriction(0.5f);
        body->setRestitution(0.1f);

        // Set T-pose orientation (no position yet; syncFromPose seeds it later).
        body->setOrient(extractOrient(bone->toDress));
        body->snapToCurrent();

        RagdollBone rb;
        rb.boneIdx       = bone->boneIdx;
        rb.body          = body.get();
        rb.capsuleOffset = bd.capsuleOffset;

        bones_.push_back(rb);
        bodies_.push_back(std::move(body));

        world.registerBody(bones_.back().body, {}, kRagdollGroup, kRagdollMask);
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
            joint = std::make_unique<ConeTwistJoint>(
                bodyA, bodyB, anchorA, anchorB,
                extractOrient(parentBone->toDress),
                extractOrient(childBone->toDress),
                jd.coneHalfAngle, jd.twistLimit);
            break;
        }

        if (!joint) continue;

        // Tag the child RagdollBone with its parent joint (non-owning view).
        for (RagdollBone& rb : bones_) {
            if (rb.boneIdx == childBone->boneIdx) {
                rb.parentJoint = joint.get();
                break;
            }
        }

        world.addJointRef(joint.get());
        joints_.push_back(std::move(joint));
    }
}

// ---------------------------------------------------------------------------
// Ragdoll::destroy
// ---------------------------------------------------------------------------

void Ragdoll::destroy(PhysicsWorld& world)
{
    // Joints first: they hold raw RigidBody* and must not outlive the bodies.
    for (auto& j : joints_)
        world.removeJointRef(j.get());
    joints_.clear();

    for (auto& b : bodies_)
        world.unregisterBody(b.get());
    bodies_.clear();

    bones_.clear();
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
// Ragdoll::activate / deactivate
// ---------------------------------------------------------------------------

void Ragdoll::activate()
{
    for (auto& body : bodies_)
        body->setMotionType(MotionType::Dynamic);
    active_ = true;
}

void Ragdoll::deactivate()
{
    for (auto& body : bodies_)
        body->setMotionType(MotionType::Kinematic);
    active_ = false;
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
        // Body centre = bone origin + capsuleOffset rotated into world.
        const mu::Vec4 localOfs(rb->capsuleOffset.x(),
                                rb->capsuleOffset.y(),
                                rb->capsuleOffset.z(), 1.f);
        const mu::Vec4 worldOfs = localOfs * boneWorldMat;
        const mu::Vec3 bodyPos(worldOfs.x(), worldOfs.y(), worldOfs.z());

        rb->body->setPos(bodyPos);
        rb->body->setOrient(extractOrient(boneWorldMat));

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
