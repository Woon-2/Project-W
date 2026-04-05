#ifndef __ContactConstraint_HPP
#define __ContactConstraint_HPP

#include "constraint.hpp"
#include "rigidBody.hpp"

// One contact point between two bodies.
// Stores warm-start accumulators (accNormal, accTangent) that persist
// within a single step across PGS iterations.
struct ContactPoint {
    mu::Vec3  worldPos;              // world-space contact location
    mu::Vec3  localA;                // worldPos - bodyA->pos() at prepare time
    mu::Vec3  localB;                // worldPos - bodyB->pos() at prepare time
    mu::NVec3 normal;                // contact normal: points from B toward A
    float     depth    = 0.f;       // penetration depth (> 0 when penetrating)
    float     accNormal      = 0.f; // accumulated normal impulse (warm start)
    float     accTangent[2]  = {};  // accumulated tangent impulses (warm start)
};

// Velocity-level Sequential Impulse constraint for one contact manifold.
// Up to 4 contact points per pair.
//
// Normal convention: ContactPoint::normal points from B toward A.
// Positive relative velocity (along normal) means separating.
// Normal impulse is non-negative (can only push bodies apart).
class ContactConstraint : public Constraint {
public:
    ContactConstraint(RigidBody* a, RigidBody* b);

    RigidBody*   bodyA = nullptr;
    RigidBody*   bodyB = nullptr;
    ContactPoint contacts[4];
    int          count = 0;

    // Append a contact point (up to 4 per constraint).
    void addContact(const ContactPoint& cp);

    // Compute effective masses, tangent directions, and Baumgarte biases.
    void prepare(Seconds dt) override;

    // Apply one PGS iteration of normal + Coulomb friction impulses.
    void solveVelocity() override;

    // Position correction is handled by the Baumgarte bias in solveVelocity.
    void solvePosition() override {}

private:
    // Per-contact cache filled by prepare().
    struct Cache {
        float    effMassNormal;
        float    effMassTangent[2];
        float    bias;        // Baumgarte velocity correction
        mu::Vec3 rA;          // worldPos - bodyA->pos()
        mu::Vec3 rB;          // worldPos - bodyB->pos()
        mu::Vec3 tangent[2];  // orthonormal tangent frame (perp to normal)
    };
    Cache cache_[4];

    // Baumgarte beta controls how fast penetration is corrected.
    static constexpr float kBaumgarteBeta = 0.2f;
    // Penetration slop: small overlaps below this threshold are ignored.
    static constexpr float kSlop          = 0.005f;
};

#endif // __ContactConstraint_HPP
