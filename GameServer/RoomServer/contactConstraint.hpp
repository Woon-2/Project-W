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
    // Set per-body external accelerations (gravity, buoyancy, etc.) that the
    // constraint solver should compensate for.  Call before prepare().
    void setExternalAccels(mu::Vec3 aA, mu::Vec3 aB);

    void solvePosition() override {}

private:
    struct Cache {
        float    effMassNormal;
        float    effMassTangent[2];
        float    bias;
        mu::Vec3 rA;
        mu::Vec3 rB;
        mu::Vec3 tangent[2];
    };
    Cache cache_[4];

    static constexpr float kBaumgarteBeta = 0.2f;
    static constexpr float kSlop          = 0.005f;

    mu::Vec3 externalAccelA_{ 0.f, 0.f, 0.f };
    mu::Vec3 externalAccelB_{ 0.f, 0.f, 0.f };
};

#endif // room_server_contactConstraint_hpp
