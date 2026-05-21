#ifndef __JointConstraint_HPP
#define __JointConstraint_HPP

#include "constraint.hpp"
#include "rigidBody.hpp"

// ---------------------------------------------------------------------------
// BallSocketJoint
//
// Removes 3 translational DOF between two bodies.
// The anchor points (in body-local space) are constrained to coincide
// in world space.  Bilateral impulse (no clamping).
//
// Usage: spine, wrist, ankle joints.
// ---------------------------------------------------------------------------
class BallSocketJoint : public Constraint {
public:
    BallSocketJoint(RigidBody* a, RigidBody* b,
                    mu::Vec3 anchorA, mu::Vec3 anchorB);

    void prepare(Seconds dt) override;
    void solveVelocity() override;
    void solvePosition() override;
    void resetAnchors() override;

private:
    RigidBody* bodyA_;
    RigidBody* bodyB_;
    mu::Vec3   anchorA_;   // pivot in bodyA local space
    mu::Vec3   anchorB_;   // pivot in bodyB local space
    float damping_;

    struct Cache {
        mu::Vec3   rA;            // world-space radius: anchor - bodyA.pos
        mu::Vec3   rB;            // world-space radius: anchor - bodyB.pos
        mu::Mat3x3 effMassInv;    // 3x3 effective mass matrix inverse
        mu::Vec3   bias;          // Baumgarte position-correction bias
        mu::Vec3   accImpulse;    // accumulated impulse for warm-starting
        mu::Vec3   pseudoBias;    // split-impulse position-correction bias
        mu::Vec3   pseudoAccImpulse;
    } cache_{};

    static constexpr float kJointBeta = 0.1f;  // Baumgarte beta (softer than contact 0.2)
    static constexpr float kSplitBeta = 0.3f;
};

// ---------------------------------------------------------------------------
// HingeJoint
//
// Removes 3 translational DOF + 2 rotational DOF, leaving a single rotation
// axis free.  Optional [minAngle, maxAngle] limit around that axis.
//
// Usage: knee, elbow joints.
// ---------------------------------------------------------------------------
class HingeJoint : public Constraint {
public:
    // axisA: hinge axis in bodyA local space (will be normalised internally).
    // minAngle / maxAngle: angular limits in radians around the hinge axis.
    HingeJoint(RigidBody* a, RigidBody* b,
               mu::Vec3 anchorA, mu::Vec3 anchorB,
               mu::Vec3 axisA,
               float minAngle = -mu::pi, float maxAngle = mu::pi);

    void prepare(Seconds dt) override;
    void solveVelocity() override;
    void solvePosition() override;
    void resetAnchors() override;

private:
    RigidBody* bodyA_;
    RigidBody* bodyB_;
    mu::Vec3  anchorA_, anchorB_;
    mu::Vec3  axisA_;              // hinge axis in bodyA local space (unit)
    mu::Vec3  axisB_;              // matching hinge axis in bodyB local space (unit)
    float     minAngle_, maxAngle_, linearDamping_, angularDamping_;
    mu::NQuat refOrient_;          // bodyB orient relative to bodyA at build time

    struct Cache {
        // Translational (BallSocket-identical):
        mu::Vec3   rA, rB;
        mu::Mat3x3 linEffMassInv;
        mu::Vec3   linBias;
        mu::Vec3   linAccImp;
        mu::Vec3   linPseudoBias;
        mu::Vec3   linPseudoAccImp;
        // Angular alignment rows (2 perpendicular to hinge axis):
        mu::Vec3   perpAxes[2];    // world-space
        float      angEffMass[2];
        float      angBias[2];
        float      angAccImp[2];
        // Limit row (1, one-sided):
        bool       limitActive;
        bool       limitLo;        // true = lower-limit violation, false = upper
        mu::Vec3   hingeAxisWorld;
        float      limitEffMass;
        float      limitBias;
        float      limitAccImp;    // clamped >= 0 (lo) or <= 0 (hi)
    } cache_{};

    static constexpr float kJointBeta = 0.1f;
};

// ---------------------------------------------------------------------------
// ConeTwistJoint
//
// Removes 3 translational DOF.  Swing (rotation away from rest axis) is
// limited to a cone of half-angle coneHalfAngle.  Twist (rotation around
// rest axis) is limited to [-twistLimit, +twistLimit].
//
// Usage: shoulder, hip joints.
// ---------------------------------------------------------------------------
class ConeTwistJoint : public Constraint {
public:
    // refOrientA / refOrientB: rest orientations of bodyA/bodyB when the joint
    // is in its neutral pose.  Limits are measured relative to these.
    // coneHalfAngle: clamp to <= pi * 0.85 to avoid gimbal singularities.
    ConeTwistJoint(RigidBody* a, RigidBody* b,
                   mu::Vec3 anchorA, mu::Vec3 anchorB,
                   mu::NQuat refOrientA, mu::NQuat refOrientB,
                   float coneHalfAngle, float twistLimit);
    ConeTwistJoint(RigidBody* a, RigidBody* b,
                   mu::Vec3 anchorA, mu::Vec3 anchorB,
                   mu::NQuat refOrientA, mu::NQuat refOrientB,
                   mu::Vec3 twistAxisLocalA,
                   float coneHalfAngle, float twistLimit);

    void prepare(Seconds dt) override;
    void solveVelocity() override;
    void solvePosition() override;
    void resetAnchors() override;

    float linearError() const { return cache_.linearError; }
    float coneViolation() const { return cache_.coneViolation; }
    float twistViolation() const { return cache_.twistViolation; }
    float coneImpulse() const { return cache_.coneAccImp; }
    float twistImpulse() const { return cache_.twistAccImp; }
    bool  coneActive() const { return cache_.coneActive; }
    bool  twistActive() const { return cache_.twistActive; }

private:
    RigidBody* bodyA_;
    RigidBody* bodyB_;
    mu::Vec3  anchorA_, anchorB_;
    mu::NQuat refOrientA_, refOrientB_;
    mu::Vec3  twistAxisLocalA_;
    float     coneHalfAngle_;
    float     twistLimit_;
    float     linearDamping_;
    float     coneDamping_;
    float     twistDamping_;

    struct Cache {
        // Translational (BallSocket-identical):
        mu::Vec3   rA, rB;
        mu::Mat3x3 linEffMassInv;
        mu::Vec3   linBias;
        mu::Vec3   linAccImp;
        mu::Vec3   linPseudoBias;
        mu::Vec3   linPseudoAccImp;
        float      linearError;
        // Cone limit (one-sided, accImp >= 0):
        bool       coneActive;
        mu::Vec3   coneAxis;       // world-space correction axis
        float      coneViolation;
        float      coneEffMass;
        float      coneBias;
        float      coneAccImp;
        float      conePseudoBias;
        float      conePseudoAccImp;
        bool       coneWarmValid;
        // Twist limit (bilateral, clamped to [-twistLimit, +twistLimit]):
        bool       twistActive;
        bool       twistLo;
        mu::Vec3   twistAxis;      // world-space twist axis
        float      twistViolation;
        float      twistEffMass;
        float      twistBias;
        float      twistAccImp;
        float      twistPseudoBias;
        float      twistPseudoAccImp;
        bool       twistWarmValid;
    } cache_{};

    // Translational correction (same as BallSocket/HingeJoint).
    static constexpr float kLinBeta   = 0.1f;
    // Angular velocity-level correction stays conservative; large limit errors
    // are handled mostly by split impulse so chains do not gain real velocity.
    static constexpr float kAngBeta   = 0.025f;
    // Position-level (split impulse) angular correction beta.
    static constexpr float kSplitBeta = 0.3f;
    static constexpr float kMaxVelCorrectionAngle   = mu::pi / 18.f; // 10 deg/step
    static constexpr float kMaxSplitCorrectionAngle = mu::pi / 9.f;  // 20 deg/step
    static constexpr float kWarmStartAxisDotMin     = 0.85f;
    static constexpr float kWarmStartScale          = 0.5f;
};

// ConeTwistJoint::solvePosition is declared non-inline and implemented in
// jointConstraint.cpp because it uses the file-scope angEff1D helper.

#endif // __JointConstraint_HPP
