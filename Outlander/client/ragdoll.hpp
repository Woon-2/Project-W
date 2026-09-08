#ifndef __Ragdoll_HPP
#define __Ragdoll_HPP

#include "rigidBody.hpp"
#include "constraint.hpp"
#include "animation.hpp"
#include "ragdollDef.hpp"

struct Skeleton;
class PhysicsWorld;

// Represents the physical bone of a single ragdoll joint.
struct RagdollBone {
    int         boneIdx     = -1;      // index into Skeleton::bones
    RigidBody*  body        = nullptr; // non-owning; owned by Ragdoll::bodies_
    Constraint* parentJoint = nullptr; // non-owning; owned by Ragdoll::joints_
                                       // null for the root bone
    mu::Vec3    capsuleOffset;         // body pos offset from bone origin (= BoneBoxDef::center)
    mu::Vec3    halfExtents;           // OBB half-extents for terrain/body collision BVH
    float       noiseImpulse = 0.f;    // max random impulse (N·s) applied at activation
};

// Bone not directly simulated by a RigidBody; rigidly follows its nearest ragdoll ancestor.
// relativeXform = finalXforms[passenger] / finalXforms[ancestor] captured at buildPassengers().
// Reconstructed each frame: finalXforms[boneIdx] = relativeXform * finalXforms[ancestorBoneIdx].
struct PassengerBone {
    int        boneIdx;          // index into Skeleton::bones
    int        ancestorBoneIdx;  // nearest ragdoll ancestor's boneIdx
    mu::Mat4x4 relativeXform;    // passenger / ancestor at capture time
};

// Manages the per-character ragdoll: creates RigidBody/Constraint instances,
// synchronises with animation, and controls the active/inactive state.
//
// Ownership model:
//   Ragdoll::bodies_ owns unique_ptr<RigidBody>.
//   Ragdoll::joints_ owns unique_ptr<Constraint>.
//   PhysicsWorld holds non-owning pointers (registerBody / addJointRef).
//
// destroy() must be called before the Ragdoll is destroyed if build() was called.
class Ragdoll {
public:
    // Ragdoll-only physics profile. Deliberately different from the live-character
    // settings: a corpse should read as a collapsing body within a second or two, which
    // real-scale gravity does not deliver for a large model.
    // Remaining tuning levers for "the collapse looks sluggish" are catalogued in
    // client/docs/ragdollSafety.md.
    static constexpr float kGravityScale  = 1.35f;   // x world gravity, applied in activate()
    static constexpr float kTopplingOmega = 2.5f;    // rad/s, applyDeathKick()

    Ragdoll()                            = default;
    ~Ragdoll()                           = default;
    Ragdoll(const Ragdoll&)              = delete;
    Ragdoll& operator=(const Ragdoll&)   = delete;
    Ragdoll(Ragdoll&&)                   = default;
    Ragdoll& operator=(Ragdoll&&)        = default;

    // Build ragdoll from skeleton + def.  Allocates bodies and joints but does NOT
    // register them with the world — registration happens in activate().
    // Call once after Object creation.
    // modelScale = model base scale x per-instance scale (= Object::scale()). Bone positions
    // are scaled via objectWorldMat (seedFromFinalXforms/syncFromPose), so it is applied only to
    // halfExtents/inertia, which do not go through boneWorldMat.
    void build(const Skeleton& skel, const RagdollDef& def, PhysicsWorld& world, mu::Vec3 modelScale);

    // Unregister all bodies/joints from world and release memory.
    // Joints are removed before bodies to prevent dangling pointer access.
    void destroy(PhysicsWorld& world);

    // Copy current animation pose into kinematic body transforms.
    // pose[bone.boneIdx] must be parent-relative AnimFrame.
    // When inactive (kinematic), call each frame to keep bodies in sync.
    // When activating, call once before activate() to seed body positions.
    void syncFromPose(const std::vector<AnimFrame>& pose, const Skeleton& skel,
                      mu::Mat4x4 objectWorldMat);

    // Seed body positions from AnimBlender::finalXformData() (post-update output).
    // finalXforms[i] = bone.toLocal * boneXformDress (same layout as finalXformData()).
    // Call after build() and before activate() when finalXformData is available.
    void seedFromFinalXforms(const std::vector<mu::Mat4x4>& finalXforms,
                             const Skeleton& skel,
                             mu::Mat4x4 objectWorldMat);

    // Read body transforms back into outPose AnimFrames (parent-relative).
    // Call each frame when active to drive character rendering.
    void syncToPose(std::vector<AnimFrame>& outPose, const Skeleton& skel,
                    mu::Mat4x4 objectWorldMat) const;

    // Overwrite finalXformData entries for ragdoll bones with current physics body positions.
    // Inverse of seedFromFinalXforms: finalXforms[i] = bone.toLocal * (boneWorldMat / objectWorldMat).
    // Call after animSystem_.update() and before rendering when ragdoll is active.
    //
    // tPhysic is the render-interpolation factor between the last two physics steps
    // (the same value every other object is drawn with). Without it a ragdoll updates in
    // 60Hz steps while the rest of the scene interpolates, which reads as stutter on
    // fast-swinging limbs -- exactly where the collapse is most visible.
    void syncToFinalXforms(std::vector<mu::Mat4x4>& finalXforms,
                            const Skeleton& skel,
                            mu::Mat4x4 objectWorldMat,
                            float tPhysic = 1.f) const;

    // Capture passenger bone bindings from the current finalXforms pose.
    // Call after seedFromFinalXforms() and before activate().
    // On re-activation, call again to refresh bindings with the new death pose.
    void buildPassengers(const Skeleton& skel, const std::vector<mu::Mat4x4>& finalXforms);

    // Register bodies/joints with world and switch all bodies to Dynamic.
    // Call seedFromFinalXforms() before this to seed positions from animation.
    void activate(PhysicsWorld& world);

    // Switch all bodies back to Kinematic and unregister from world.
    void deactivate(PhysicsWorld& world);

    // Hand the ragdoll the momentum of the character it replaced, then kick it over.
    // Call once, right after activate().
    //   1) Every bone inherits initVel (the rigid character's velocity at the killing
    //      blow) -- momentum is conserved regardless of per-bone mass.
    //   2) A toppling rotation is added as a CONSISTENT RIGID MOTION about a ground
    //      pivot: v_i += cross(omega, pos_i - pivot), omega_i = omega. Because the whole
    //      assembly shares one rigid velocity field, every joint sees zero relative
    //      velocity, so the body starts falling over without the solver fighting it.
    //   3) Per-bone noise impulses applied OFF the centre of mass add the small
    //      asymmetric spin that makes the collapse read as physics rather than a canned
    //      animation.
    void applyDeathKick(mu::Vec3 initVel);

    bool isActive() const { return active_; }
    bool isBuilt()  const { return !bodies_.empty(); }

    const std::vector<RagdollBone>& bones() const { return bones_; }
    std::vector<RagdollBone>&       bones()       { return bones_; }

private:
    // Recursive DFS helper for syncFromPose.
    void syncFromPoseDFS(const Bone* bone,
                         mu::Mat4x4 parentWorldMat,
                         const std::vector<AnimFrame>& pose) const;

    // Recursive DFS helper for syncToPose.
    // parentWorldMat: world transform of the parent body (not the parent bone's world!).
    void syncToPoseDFS(const Bone* bone,
                       mu::Mat4x4 parentBodyWorldMat,
                       std::vector<AnimFrame>& outPose) const;

    // Recursive DFS helper for buildPassengers.
    void buildPassengersDFS(const Bone* bone, int currentAncestorIdx,
                             const std::vector<mu::Mat4x4>& finalXforms);

    // Returns the RagdollBone for the given boneIdx, or nullptr if not found.
    const RagdollBone* findBone(int boneIdx) const;

    std::vector<RagdollBone>                   bones_;
    std::vector<std::unique_ptr<RigidBody>>    bodies_;   // owns memory
    std::vector<std::unique_ptr<Constraint>>   joints_;      // owns memory
    std::vector<std::pair<RigidBody*, RigidBody*>> jointBodies_;  // parallel to joints_; built in build()
    std::vector<std::pair<RigidBody*, RigidBody*>> ignoredPairs_; // registered in activate(); cleared in deactivate()/destroy()
    std::vector<PassengerBone>                 passengers_;
    // model base x per-instance scale (captured in build()); applied to ragdoll geometry so the
    // skinned mesh stays scaled after activation (syncToFinalXforms) and bodies sit on the scaled
    // skeleton (seed/syncFromPose use scaled capsuleOffset).
    mu::Vec3 modelScale_{ 1.f, 1.f, 1.f };
    bool active_ = false;
};

#endif // __Ragdoll_HPP
