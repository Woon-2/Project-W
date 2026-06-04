#ifndef room_server_contactConstraint_hpp
#define room_server_contactConstraint_hpp

#include "constraint.hpp"
#include "rigidBody.hpp"

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

    void addContact(const ContactPoint& cp);

    void prepare(Seconds dt) override;
    void solveVelocity() override;
    void applyWarmStart();
    // Set per-body external accelerations (gravity, buoyancy, etc.) that the
    // constraint solver should compensate for.  Call before prepare().
    void setExternalAccels(mu::Vec3 aA, mu::Vec3 aB);

    // Mark this constraint as a body-terrain contact.
    // Terrain contacts skip the Baumgarte bias to prevent ground vibration.
    // Position correction is handled exclusively by Split Impulse.
    void setTerrainContact(bool v) { isTerrainContact_ = v; }

    void solvePosition() override;

private:
    struct Cache {
        float    effMassNormal;
        float    effMassTangent[2];
        float    bias;             // velocity-level: external-force compensation only
        float    pseudoBias;       // position-level: β/dt * penetration
        float    accNormalPseudo;  // accumulated pseudo-impulse this step (reset in prepare)
        mu::Vec3 rA;
        mu::Vec3 rB;
        mu::Vec3 tangent[2];
    };
    Cache cache_[4];

    // Penetration recovery split between two channels (tune the RATIO):
    //   kBaumgarteBeta    (velocity-level): real separating velocity, bleeds penetration
    //     out gradually -> smooth, no pop. Disabled for terrain (vibration).
    //   kSplitImpulseBeta (position-level): energy-neutral pseudo-velocity to position;
    //     high values snap out penetration in one step (poppy). Bullet erp2 = 0.8.
    // Pure split-impulse was visibly poppy; pure Baumgarte leaves residual penetration.
    // Must match the client values to keep prediction and authority in sync.
    static constexpr float kBaumgarteBeta    = 0.2f;
    static constexpr float kSplitImpulseBeta = 0.3f;
    // External-force (gravity) compensation is shared across BOTH channels (velocity
    // bias + split-impulse pseudoBias). The two shares must total exactly extComp;
    // applying the full extComp on each double-compensates gravity and the body floats
    // up. kExtCompVelFrac is the velocity-channel share (the split channel gets the rest).
    static constexpr float kExtCompVelFrac   = 0.5f;
    static constexpr float kSlop             = 0.005f;

    bool     isTerrainContact_ = false;
    mu::Vec3 externalAccelA_{ 0.f, 0.f, 0.f };
    mu::Vec3 externalAccelB_{ 0.f, 0.f, 0.f };
};

#endif // room_server_contactConstraint_hpp
