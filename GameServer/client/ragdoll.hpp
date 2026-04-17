#ifndef __Ragdoll_HPP
#define __Ragdoll_HPP

#include "rigidBody.hpp"
#include "constraint.hpp"
#include "animation.hpp"
#include "ragdollDef.hpp"
#include <memory>
#include <vector>

struct Skeleton;
class PhysicsWorld;

// Represents the physical bone of a single ragdoll joint.
struct RagdollBone {
    int         boneIdx     = -1;      // index into Skeleton::bones
    RigidBody*  body        = nullptr; // non-owning; owned by Ragdoll::bodies_
    Constraint* parentJoint = nullptr; // non-owning; owned by Ragdoll::joints_
                                       // null for the root bone
    mu::Vec3    capsuleOffset;         // capsule centre offset in body local space
                                       // (same as BoneCapsuleDef::capsuleOffset)
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
    Ragdoll()                            = default;
    ~Ragdoll()                           = default;
    Ragdoll(const Ragdoll&)              = delete;
    Ragdoll& operator=(const Ragdoll&)   = delete;
    Ragdoll(Ragdoll&&)                   = default;
    Ragdoll& operator=(Ragdoll&&)        = default;

    // Build ragdoll from skeleton + def.  Registers bodies and joints with world.
    // Call once after Object creation.
    void build(const Skeleton& skel, const RagdollDef& def, PhysicsWorld& world);

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

    // Switch all Dynamic bodies from Kinematic to Dynamic.
    // Call syncFromPose() before this to seed positions from animation.
    void activate();

    // Switch all Dynamic bodies back to Kinematic.
    void deactivate();

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

    // Returns the RagdollBone for the given boneIdx, or nullptr if not found.
    const RagdollBone* findBone(int boneIdx) const;

    std::vector<RagdollBone>                   bones_;
    std::vector<std::unique_ptr<RigidBody>>    bodies_;   // owns memory
    std::vector<std::unique_ptr<Constraint>>   joints_;   // owns memory
    bool active_ = false;
};

#endif // __Ragdoll_HPP
