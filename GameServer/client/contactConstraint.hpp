#ifndef __ContactConstraint_HPP
#define __ContactConstraint_HPP

#include "constraint.hpp"
#include "rigidBody.hpp"
// ContactPoint is defined in collision.hpp (included transitively via rigidBody.hpp)

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

    // Apply warm-start impulses (previous step's accumulated values) to bodies.
    // Call after prepare() and before the first solveVelocity() iteration.
    void applyWarmStart();

    // Set per-body external accelerations (gravity, buoyancy, etc.) that the
    // constraint solver should compensate for.  Call before prepare().
    void setExternalAccels(mu::Vec3 aA, mu::Vec3 aB);

    // Mark this constraint as a body-terrain contact.
    // Terrain contacts skip the Baumgarte bias to prevent the artificial upward
    // velocity that causes visible ground vibration. Position correction is
    // handled exclusively by Split Impulse.
    void setTerrainContact(bool v) { isTerrainContact_ = v; }

    // True if this constraint is a body-vs-terrain support contact. The game
    // layer reads this (via forEachContact) to decide whether a character is
    // grounded for gravity gating.
    bool isTerrainContact() const { return isTerrainContact_; }

    // Split Impulse position correction: accumulates pseudo-velocities that are
    // applied to body positions at the end of the step (never to real velocity).
    void solvePosition() override;

private:
    // Per-contact cache filled by prepare().
    struct Cache {
        float    effMassNormal;
        float    effMassTangent[2];
        float    bias;             // velocity-level: external-force compensation only
        float    pseudoBias;       // position-level: β/dt * penetration
        float    accNormalPseudo;  // accumulated pseudo-impulse this step (reset in prepare)
        mu::Vec3 rA;               // worldPos - bodyA->pos()
        mu::Vec3 rB;               // worldPos - bodyB->pos()
        mu::Vec3 tangent[2];       // orthonormal tangent frame (perp to normal)
    };
    Cache cache_[4];

    // --- Penetration recovery: Baumgarte + Split Impulse blend ---
    // The two betas split the penetration-recovery work between two channels.
    // Tune their RATIO to trade smoothness against firmness:
    //
    //   kBaumgarteBeta    (velocity-level): injects REAL separating velocity that
    //     bleeds the penetration out gradually over several frames -> smooth, no
    //     visible "pop", but can slightly over-separate. Disabled for terrain
    //     (see isTerrainContact_) where the injected upward velocity makes the
    //     body briefly leave the ground (visible vibration).
    //   kSplitImpulseBeta (position-level): corrects penetration via pseudo-velocity
    //     applied directly to position. Energy-neutral (never touches real velocity),
    //     but a high value resolves ~beta of the depth in a SINGLE step -> a hard
    //     snap that looks "poppy" on deep penetration. Bullet's erp2 default is 0.8;
    //     we use a lower value and let Baumgarte smooth the rest.
    //
    // Pure split-impulse (Baumgarte=0, beta=0.8) was visibly poppy; pure Baumgarte
    // leaves residual penetration. The blend below is smooth yet firm.
    static constexpr float kBaumgarteBeta    = 0.2f;
    static constexpr float kSplitImpulseBeta = 0.3f;
    // Penetration slop: small overlaps below this threshold are ignored.
    static constexpr float kSlop             = 0.005f;
    // Max penetration depth (m) fed into the Baumgarte/split correction per step.
    // Correction velocity = beta * invDt * depth, so an uncapped deep overlap (e.g. a
    // 4x-scaled ragdoll limb sunk into the torso) produces an enormous separating
    // velocity that overflows to Inf -> NaN inside the velocity iterations. Capping the
    // depth bounds the per-step correction (deep penetration just resolves over several
    // steps). Matches the staticDepenetration clamp philosophy (kMaxCorrect=0.2m).
    static constexpr float kMaxCorrectionDepth = 0.2f;

    // External-force (gravity) compensation is shared across BOTH the velocity bias
    // and the split-impulse pseudoBias (see prepare()). The two shares must total
    // exactly extComp: applying the full extComp on each channel double-compensates
    // gravity, so the integrate-step sink (applied once) is cancelled twice and the
    // body slowly floats up. kExtCompVelFrac is the velocity-channel share; the split
    // channel gets the rest. For any value the gravity terms still cancel (no creep,
    // no float); the split only changes how much gravity comp is real velocity vs
    // energy-neutral position correction.
    static constexpr float kExtCompVelFrac   = 0.5f;

    bool     isTerrainContact_ = false;
    mu::Vec3 externalAccelA_{ 0.f, 0.f, 0.f };
    mu::Vec3 externalAccelB_{ 0.f, 0.f, 0.f };
};

#endif // __ContactConstraint_HPP
