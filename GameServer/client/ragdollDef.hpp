#ifndef __RagdollDef_HPP
#define __RagdollDef_HPP

#include <vector>
#include <string>
#include "../common/mathUtil.hpp"

enum class JointType { BallSocket, Hinge, ConeTwist };

// Physical representation of one bone as an OBB.
struct BoneBoxDef {
    std::string bodyName;
    std::string boneName;
    mu::Vec3 halfExtents;     // half-size of the box (bone-local)
    mu::Vec3 center;          // offset from bone origin to box centre (bone-local)
    mu::Vec3 rotEuler;        // bone-local box orientation in degrees (XYZ Euler, reserved for future collision shape)
    float    mass;
};

// Joint connecting parentBone to childBone.
struct JointDef {
    std::string parentBodyName;
    std::string childBodyName;
    std::string parentBoneName;
    std::string childBoneName;
    JointType type;
    // Hinge fields (ignored for BallSocket / ConeTwist):
    mu::Vec3 axisLocalA;              // hinge axis in parent-bone local space
    float minAngle = -mu::pi;
    float maxAngle =  mu::pi;
    // ConeTwist fields (ignored for BallSocket / Hinge):
    float coneHalfAngle = mu::half_pi;
    float twistLimit    = mu::pi * 0.25f;
};

// A complete ragdoll definition: set of OBB bones + joints between them.
struct RagdollDef {
    std::vector<BoneBoxDef> bones;
    std::vector<JointDef>   joints;
};

// Predefined humanoid ragdoll.
// The bone names must match the skeleton exported from Unity.
const RagdollDef& getHumanoidRagdollDef();

#endif // __RagdollDef_HPP
