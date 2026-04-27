using UnityEngine;
using System.Collections.Generic;

// Matches JointType in ragdollDef.hpp
public enum RagdollJointType { BallSocket, Hinge, ConeTwist }

// Matches BoneCapsuleDef in ragdollDef.hpp.
// Anchors and ref-orientations are derived from the T-pose skeleton on the C++ side,
// so they are not stored here.
[System.Serializable]
public class RagdollBody
{
    public string    name          = "Body";
    public Transform bone;                              // scene reference for gizmos
    public Vector3   capsuleOffset = Vector3.zero;     // bone-local, matches BoneCapsuleDef::capsuleOffset
    public float     capsuleRadius = 0.1f;             // matches BoneCapsuleDef::radius
    public float     halfHeight    = 0.15f;            // matches BoneCapsuleDef::halfHeight (total height = halfHeight*2)
    public int       capsuleDir    = 1;                // 0=X 1=Y 2=Z  -- gizmo only, not exported
    public float     mass          = 1.0f;             // matches BoneCapsuleDef::mass
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
    public GameObject          skeletonRoot;
    public List<RagdollBody>   bodies = new List<RagdollBody>();
    public List<RagdollJoint>  joints = new List<RagdollJoint>();
}
