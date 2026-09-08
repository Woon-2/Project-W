#include "pch.hpp"
#include "ragdollDef.hpp"

// ---------------------------------------------------------------------------
// Humanoid ragdoll definition.
//
// Bone names follow the Unity Humanoid skeleton convention.
// Box dimensions are approximate and should be tuned per character.
// ---------------------------------------------------------------------------

const RagdollDef& getHumanoidRagdollDef()
{
    static const RagdollDef kDef{
        .bones = {
            // boneName              halfExtents (X,Y,Z)           center (bone-local)       rotEuler  mass
            { "Hips",          "Hips",         { 0.12f, 0.08f, 0.10f }, { 0.f,  0.f,   0.f }, {}, 12.f },
            { "Spine",         "Spine",        { 0.10f, 0.10f, 0.09f }, { 0.f,  0.10f, 0.f }, {},  8.f },
            { "Chest",         "Chest",        { 0.11f, 0.10f, 0.09f }, { 0.f,  0.10f, 0.f }, {},  8.f },
            { "Head",          "Head",         { 0.10f, 0.10f, 0.10f }, { 0.f,  0.10f, 0.f }, {},  5.f },
            { "LeftUpperArm",  "LeftUpperArm", { 0.06f, 0.14f, 0.06f }, { 0.f, -0.14f, 0.f }, {},  3.f },
            { "LeftLowerArm",  "LeftLowerArm", { 0.05f, 0.12f, 0.05f }, { 0.f, -0.12f, 0.f }, {},  2.f },
            { "RightUpperArm", "RightUpperArm",{ 0.06f, 0.14f, 0.06f }, { 0.f, -0.14f, 0.f }, {},  3.f },
            { "RightLowerArm", "RightLowerArm",{ 0.05f, 0.12f, 0.05f }, { 0.f, -0.12f, 0.f }, {},  2.f },
            { "LeftUpperLeg",  "LeftUpperLeg", { 0.08f, 0.20f, 0.08f }, { 0.f, -0.20f, 0.f }, {},  8.f },
            { "LeftLowerLeg",  "LeftLowerLeg", { 0.06f, 0.18f, 0.06f }, { 0.f, -0.18f, 0.f }, {},  5.f },
            { "RightUpperLeg", "RightUpperLeg",{ 0.08f, 0.20f, 0.08f }, { 0.f, -0.20f, 0.f }, {},  8.f },
            { "RightLowerLeg", "RightLowerLeg",{ 0.06f, 0.18f, 0.06f }, { 0.f, -0.18f, 0.f }, {},  5.f },
        },
        .joints = {
            // Spine chain
            {
                .parentBodyName = "Hips",
                .parentBoneName = "Hips",
                .childBoneName  = "Spine",
                .type           = JointType::ConeTwist,
                .coneHalfAngle  = mu::pi * 0.2f,
                .twistLimit     = mu::pi * 0.15f,
            },
            {
                .parentBodyName = "Spine",
                .parentBoneName = "Spine",
                .childBoneName  = "Chest",
                .type           = JointType::ConeTwist,
                .coneHalfAngle  = mu::pi * 0.2f,
                .twistLimit     = mu::pi * 0.15f,
            },
            {
                .parentBodyName = "Chest",
                .parentBoneName = "Chest",
                .childBoneName  = "Head",
                .type           = JointType::ConeTwist,
                .coneHalfAngle  = mu::pi * 0.3f,
                .twistLimit     = mu::pi * 0.2f,
            },
            // Left arm
            {
                .parentBodyName = "Chest",
                .parentBoneName = "Chest",
                .childBoneName  = "LeftUpperArm",
                .type           = JointType::ConeTwist,
                .coneHalfAngle  = mu::pi * 0.6f,
                .twistLimit     = mu::pi * 0.5f,
            },
            {
                .parentBodyName = "LeftUpperArm",
                .parentBoneName = "LeftUpperArm",
                .childBoneName  = "LeftLowerArm",
                .type           = JointType::Hinge,
                .axisLocalA     = { 1.f, 0.f, 0.f },
                .minAngle       = 0.f,
                .maxAngle       = mu::pi * 0.9f,
            },
            // Right arm
            {
                .parentBodyName = "Chest",
                .parentBoneName = "Chest",
                .childBoneName  = "RightUpperArm",
                .type           = JointType::ConeTwist,
                .coneHalfAngle  = mu::pi * 0.6f,
                .twistLimit     = mu::pi * 0.5f,
            },
            {
                .parentBodyName = "RightUpperArm",
                .parentBoneName = "RightUpperArm",
                .childBoneName  = "RightLowerArm",
                .type           = JointType::Hinge,
                .axisLocalA     = { 1.f, 0.f, 0.f },
                .minAngle       = 0.f,
                .maxAngle       = mu::pi * 0.9f,
            },
            // Left leg
            {
                .parentBodyName = "Hips",
                .parentBoneName = "Hips",
                .childBoneName  = "LeftUpperLeg",
                .type           = JointType::ConeTwist,
                .coneHalfAngle  = mu::pi * 0.5f,
                .twistLimit     = mu::pi * 0.25f,
            },
            {
                .parentBodyName = "LeftUpperLeg",
                .parentBoneName = "LeftUpperLeg",
                .childBoneName  = "LeftLowerLeg",
                .type           = JointType::Hinge,
                .axisLocalA     = { 1.f, 0.f, 0.f },
                .minAngle       = 0.f,
                .maxAngle       = mu::pi * 0.9f,
            },
            // Right leg
            {
                .parentBodyName = "Hips",
                .parentBoneName = "Hips",
                .childBoneName  = "RightUpperLeg",
                .type           = JointType::ConeTwist,
                .coneHalfAngle  = mu::pi * 0.5f,
                .twistLimit     = mu::pi * 0.25f,
            },
            {
                .parentBodyName = "RightUpperLeg",
                .parentBoneName = "RightUpperLeg",
                .childBoneName  = "RightLowerLeg",
                .type           = JointType::Hinge,
                .axisLocalA     = { 1.f, 0.f, 0.f },
                .minAngle       = 0.f,
                .maxAngle       = mu::pi * 0.9f,
            },
        }
    };
    return kDef;
}
