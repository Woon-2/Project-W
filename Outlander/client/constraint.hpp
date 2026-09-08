#ifndef __Constraint_HPP
#define __Constraint_HPP

#include "rigidBody.hpp"
#include <algorithm>
#include <limits>

struct JacobianRow {
    RigidBody* bodyA = nullptr;
    RigidBody* bodyB = nullptr;
    mu::Vec3   linA{};
    mu::Vec3   angA{};
    mu::Vec3   linB{};
    mu::Vec3   angB{};
    float      effMass = 0.f;
    float      rhs = 0.f; // Solves Jv + rhs = 0.
    float      lower = -std::numeric_limits<float>::infinity();
    float      upper =  std::numeric_limits<float>::infinity();
    bool       pseudo = false;
};

// sor: successive over/under-relaxation factor (default 1.0).
// Passing sor < 1.0 scales the applied delta, preventing per-iteration
// overshoot in branching joint structures.  accImpulse is updated by the
// scaled amount so warm-starting reflects what was actually applied.
float solveJacobianRow(const JacobianRow& row, float& accImpulse, float sor = 1.f);

// Abstract base for all velocity-level + position-level constraints.
// Concrete types: ContactConstraint, BallSocketJoint, HingeJoint, ConeTwistJoint.
//
// The PhysicsWorld::step() pipeline calls these in order:
//   1. prepare(dt)     - cache effective masses, compute biases
//   2. solveVelocity() - PGS velocity-level impulse (called N times)
//   3. solvePosition() - position-level correction (called once, split impulse)
class Constraint {
public:
    static constexpr float linearCFM = 1e-3f;
    static constexpr float angularCFM = 1e-2f;

    virtual ~Constraint() = default;

    // Pre-compute cached data (effective mass, bias) for this step.
    // Called once per step before any PGS iteration.
    virtual void prepare(Seconds dt) = 0;

    // Apply one iteration of velocity-level impulses.
    // Called solverIterations times per step.
    virtual void solveVelocity() = 0;

    // Apply position-level correction (split impulse / pseudo-velocity).
    // Called once per step after all velocity iterations.
    virtual void solvePosition() = 0;

    // Recompute anchorA from the current body positions so joint pivots are
    // satisfied at the seeded (animation) pose.  Call after seedFromFinalXforms()
    // and before activate() to prevent large initial Baumgarte corrections.
    virtual void resetAnchors() {}
};

#endif // __Constraint_HPP
