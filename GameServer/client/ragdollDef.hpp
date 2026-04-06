#ifndef __RagdollDef_HPP
#define __RagdollDef_HPP

#include <span>
#include <string_view>
#include "../common/mathUtil.hpp"

enum class JointType { BallSocket, Hinge, ConeTwist };

// Physical representation of one bone as a capsule.
struct BoneCapsuleDef {
    std::string_view boneName;
    float radius;
    float halfHeight;
    float mass;
    mu::Vec3 capsuleOffset;   // offset from bone origin to capsule centre (bone-local)
};

// Joint connecting parentBone to childBone.
struct JointDef {
    std::string_view parentBoneName;
    std::string_view childBoneName;
    JointType type;
    // Hinge fields (ignored for BallSocket / ConeTwist):
    mu::Vec3 axisLocalA;              // hinge axis in parent-bone local space
    float minAngle = -mu::pi;
    float maxAngle =  mu::pi;
    // ConeTwist fields (ignored for BallSocket / Hinge):
    float coneHalfAngle = mu::half_pi;
    float twistLimit    = mu::pi * 0.25f;
};

// A complete ragdoll definition: set of capsule bones + joints between them.
struct RagdollDef {
    std::span<const BoneCapsuleDef> bones;
    std::span<const JointDef>       joints;
};

// Predefined humanoid ragdoll.
// The bone names must match the skeleton exported from Unity.
const RagdollDef& getHumanoidRagdollDef();

#endif // __RagdollDef_HPP
