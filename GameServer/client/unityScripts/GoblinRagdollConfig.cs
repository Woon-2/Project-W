using UnityEngine;
using System.Collections.Generic;

// Matches JointType in ragdollDef.hpp
public enum RagdollJointType { BallSocket, Hinge, ConeTwist }

// Matches BoneBoxDef in ragdollDef.hpp.
// Anchors and ref-orientations are derived from the T-pose skeleton on the C++ side,
// so they are not stored here.
[System.Serializable]
public class RagdollBody
{
    public string    name          = "Body";
    public Transform bone;                                                    // scene reference for gizmos
    public Vector3   center        = Vector3.zero;                            // bone-local box centre, matches BoneBoxDef::center
    public Vector3   halfExtents   = new Vector3(0.1f, 0.15f, 0.1f);        // matches BoneBoxDef::halfExtents
    public Vector3   rotationEuler = Vector3.zero;                            // bone-local box rotation, matches BoneBoxDef::rotEuler
    public float     mass          = 1.0f;                                    // matches BoneBoxDef::mass
}

// Matches JointDef in ragdollDef.hpp.
[System.Serializable]
public class RagdollJoint
{
    public string         parentBoneName = "";
    public string         childBoneName  = "";
    public RagdollJointType jointType    = RagdollJointType.ConeTwist;

    // --- Hinge (ignored for BallSocket / ConeTwist) ---
    // Axis in parent-bone local space, matches JointDef::axisLocalA
    public Vector3 hingeAxisLocal = Vector3.right;
    public float   minAngleDeg    = -90f;   // converted to radians on export
    public float   maxAngleDeg    =  90f;

    // --- ConeTwist (ignored for BallSocket / Hinge) ---
    // Matches JointDef::coneHalfAngle and JointDef::twistLimit
    public float   coneHalfAngleDeg = 45f;  // converted to radians on export
    public float   twistLimitDeg    = 30f;
}

public class GoblinRagdollConfig : MonoBehaviour
{
    public GameObject           skeletonRoot;
    public MultiBoundingVolume  mbvSource;
    public List<RagdollBody>    bodies = new List<RagdollBody>();
    public List<RagdollJoint>   joints = new List<RagdollJoint>();
}
