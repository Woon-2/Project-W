#include "pch.hpp"
#include "ragdollDef.hpp"

// ---------------------------------------------------------------------------
// Humanoid ragdoll definition.
//
// Bone names follow the Unity Humanoid skeleton convention.
// Capsule dimensions are approximate and should be tuned per character.
// ---------------------------------------------------------------------------

static const BoneCapsuleDef kHumanoidBones[] = {
    // boneName              radius  halfH   mass   capsuleOffset (bone-local)
    { "Hips",                0.12f,  0.08f,  12.f,  {0.f, 0.f,  0.f} },
    { "Spine",               0.10f,  0.10f,   8.f,  {0.f, 0.1f, 0.f} },
    { "Chest",               0.11f,  0.10f,   8.f,  {0.f, 0.1f, 0.f} },
    { "Head",                0.10f,  0.10f,   5.f,  {0.f, 0.1f, 0.f} },
    { "LeftUpperArm",        0.06f,  0.14f,   3.f,  {0.f,-0.14f,0.f} },
    { "LeftLowerArm",        0.05f,  0.12f,   2.f,  {0.f,-0.12f,0.f} },
    { "RightUpperArm",       0.06f,  0.14f,   3.f,  {0.f,-0.14f,0.f} },
    { "RightLowerArm",       0.05f,  0.12f,   2.f,  {0.f,-0.12f,0.f} },
    { "LeftUpperLeg",        0.08f,  0.20f,   8.f,  {0.f,-0.20f,0.f} },
    { "LeftLowerLeg",        0.06f,  0.18f,   5.f,  {0.f,-0.18f,0.f} },
    { "RightUpperLeg",       0.08f,  0.20f,   8.f,  {0.f,-0.20f,0.f} },
    { "RightLowerLeg",       0.06f,  0.18f,   5.f,  {0.f,-0.18f,0.f} },
};

static const JointDef kHumanoidJoints[] = {
    // Spine chain
    {
        .parentBoneName = "Hips",
        .childBoneName  = "Spine",
        .type           = JointType::ConeTwist,
        .coneHalfAngle  = mu::pi * 0.2f,
        .twistLimit     = mu::pi * 0.15f,
    },
    {
        .parentBoneName = "Spine",
        .childBoneName  = "Chest",
        .type           = JointType::ConeTwist,
        .coneHalfAngle  = mu::pi * 0.2f,
        .twistLimit     = mu::pi * 0.15f,
    },
    {
        .parentBoneName = "Chest",
        .childBoneName  = "Head",
        .type           = JointType::ConeTwist,
        .coneHalfAngle  = mu::pi * 0.3f,
        .twistLimit     = mu::pi * 0.2f,
    },
    // Left arm
    {
        .parentBoneName = "Chest",
        .childBoneName  = "LeftUpperArm",
        .type           = JointType::ConeTwist,
        .coneHalfAngle  = mu::pi * 0.6f,
        .twistLimit     = mu::pi * 0.5f,
    },
    {
        .parentBoneName  = "LeftUpperArm",
        .childBoneName   = "LeftLowerArm",
        .type            = JointType::Hinge,
        .axisLocalA      = {1.f, 0.f, 0.f},
        .minAngle        = 0.f,
        .maxAngle        = mu::pi * 0.9f,
    },
    // Right arm
    {
        .parentBoneName = "Chest",
        .childBoneName  = "RightUpperArm",
        .type           = JointType::ConeTwist,
        .coneHalfAngle  = mu::pi * 0.6f,
        .twistLimit     = mu::pi * 0.5f,
    },
    {
        .parentBoneName  = "RightUpperArm",
        .childBoneName   = "RightLowerArm",
        .type            = JointType::Hinge,
        .axisLocalA      = {1.f, 0.f, 0.f},
        .minAngle        = 0.f,
        .maxAngle        = mu::pi * 0.9f,
    },
    // Left leg
    {
        .parentBoneName = "Hips",
        .childBoneName  = "LeftUpperLeg",
        .type           = JointType::ConeTwist,
        .coneHalfAngle  = mu::pi * 0.5f,
        .twistLimit     = mu::pi * 0.25f,
    },
    {
        .parentBoneName  = "LeftUpperLeg",
        .childBoneName   = "LeftLowerLeg",
        .type            = JointType::Hinge,
        .axisLocalA      = {1.f, 0.f, 0.f},
        .minAngle        = 0.f,
        .maxAngle        = mu::pi * 0.9f,
    },
    // Right leg
    {
        .parentBoneName = "Hips",
        .childBoneName  = "RightUpperLeg",
        .type           = JointType::ConeTwist,
        .coneHalfAngle  = mu::pi * 0.5f,
        .twistLimit     = mu::pi * 0.25f,
    },
    {
        .parentBoneName  = "RightUpperLeg",
        .childBoneName   = "RightLowerLeg",
        .type            = JointType::Hinge,
        .axisLocalA      = {1.f, 0.f, 0.f},
        .minAngle        = 0.f,
        .maxAngle        = mu::pi * 0.9f,
    },
};

const RagdollDef& getHumanoidRagdollDef()
{
    static const RagdollDef kDef{
        .bones  = kHumanoidBones,
        .joints = kHumanoidJoints,
    };
    return kDef;
}
