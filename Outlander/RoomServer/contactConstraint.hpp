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
    bool isTerrainContact() const  { return isTerrainContact_; }

    // Mark this constraint as an agent-vs-agent (character) contact. Set by
    // generateContacts() in the SAME branch that projects the normal onto the
    // horizontal plane, so prepare() may assume the normal has no Y component.
    // Such a contact gets a single horizontal friction axis (see prepare()).
    void setCharacterContact(bool v) { isCharacterContact_ = v; }
    bool isCharacterContact() const  { return isCharacterContact_; }

    void solvePosition() override;

private:
    struct Cache {
        float    effMassNormal;
        float    effMassTangent[2];
        float    bias;             // velocity-level: external-force compensation only
        float    pseudoBias;       // position-level: β/dt * penetration
        float    accNormalPseudo;  // accumulated pseudo-impulse this step (reset in prepare)
        int      tangentCount;     // 2 normally, 1 for character contacts (horizontal only)
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
    // [MIRROR] client/contactConstraint.hpp kBaumgarteBeta / kSplitImpulseBeta — must stay equal.
    static constexpr float kBaumgarteBeta    = 0.2f;
    static constexpr float kSplitImpulseBeta = 0.3f;
    // External-force (gravity) compensation is shared across BOTH channels (velocity
    // bias + split-impulse pseudoBias). The two shares must total exactly extComp;
    // applying the full extComp on each double-compensates gravity and the body floats
    // up. kExtCompVelFrac is the velocity-channel share (the split channel gets the rest).
    // [MIRROR] client/contactConstraint.hpp kExtCompVelFrac / kSlop — must stay equal.
    static constexpr float kExtCompVelFrac   = 0.5f;
    static constexpr float kSlop             = 0.005f;
    // Max penetration depth (m) fed into the Baumgarte/split correction per step.
    // Correction velocity = beta * invDt * depth, so an uncapped deep overlap produces
    // an enormous correction: a player walking into the boss's bone-box BVH reaches
    // depths of several tenths of a metre, and the split-impulse channel then TELEPORTS
    // the body (pseudo-velocity writes position directly, no velocity change) — the boss
    // pops upright into the air and free-falls back. Capping the depth bounds the
    // per-step correction; deep penetration just resolves over several steps.
    // Matches the staticDepenetration clamp philosophy (kMaxCorrect=0.2m).
    // [MIRROR] client/contactConstraint.hpp kMaxCorrectionDepth — must stay equal.
    static constexpr float kMaxCorrectionDepth = 0.2f;

    bool     isTerrainContact_   = false;
    bool     isCharacterContact_ = false;
    mu::Vec3 externalAccelA_{ 0.f, 0.f, 0.f };
    mu::Vec3 externalAccelB_{ 0.f, 0.f, 0.f };
};

#endif // room_server_contactConstraint_hpp
