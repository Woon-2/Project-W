#include "rspch.hpp"
#include "contactConstraint.hpp"

ContactConstraint::ContactConstraint(RigidBody* a, RigidBody* b)
    : bodyA(a), bodyB(b)
{}

void ContactConstraint::setExternalAccels(mu::Vec3 aA, mu::Vec3 aB)
{
    externalAccelA_ = aA;
    externalAccelB_ = aB;
}

void ContactConstraint::addContact(const ContactPoint& cp)
{
    if (count < 4)
        contacts[count++] = cp;
}

// Build a tangent frame perpendicular to n.
// Returns two orthonormal vectors tangent[0] and tangent[1] that span the
// contact plane (both perpendicular to n and to each other).
static void buildTangentFrame(mu::NVec3 normal, mu::Vec3 outTangent[2])
{
    const mu::Vec3 n = normal;
    // Choose a reference vector that is not parallel to n.
    mu::Vec3 ref = (std::abs(n.x()) < 0.57735f)
                   ? mu::Vec3(1.f, 0.f, 0.f)
                   : mu::Vec3(0.f, 1.f, 0.f);

    outTangent[0] = mu::Vec3(mu::normalize(mu::cross(ref, n)));
    outTangent[1] = mu::Vec3(mu::normalize(mu::cross(n, outTangent[0])));
}

// Compute the angular contribution to effective mass along direction d at radius r.
// Returns dot(r x d, (r x d) * invInertiaWorld).
static float angularEffectiveMass(mu::Vec3 r, mu::Vec3 d,
                                   const mu::Mat3x3& invI)
{
    const mu::Vec3 rxd = mu::cross(r, d);
    return mu::dot(rxd, rxd * invI);
}

void ContactConstraint::prepare(Seconds dt)
{
    const float dtf = dt.count();
    const float invDt = (dtf > 0.f) ? (1.f / dtf) : 0.f;

    for (int i = 0; i < count; ++i) {
        const ContactPoint& cp = contacts[i];
        Cache& c = cache_[i];

        c.rA = cp.worldPos - bodyA->pos();
        c.rB = cp.worldPos - bodyB->pos();

        const mu::Vec3 n = cp.normal;

        // Character contacts: generateContacts() projected the normal onto the
        // horizontal plane, so the tangent plane necessarily contains world Y and
        // buildTangentFrame() hands back one tangent lying exactly along +/-Y
        // (n=(nx,0,nz) yields tangent (0,-1,0) or (0,1,0) depending on the branch).
        // The friction row along that tangent drives the two bodies' VERTICAL
        // relative velocity to zero, i.e. an overlapping character is dragged along
        // by the other's knockback / jump / fall: a player launched by a hit lifts
        // the boss he is standing inside. Horizontalizing the normal only stopped
        // the push, not the drag. Upright agents therefore get exactly one friction
        // axis: the horizontal perpendicular of n.
        c.tangentCount = 2;
        if (isCharacterContact_) {
            const mu::Vec3  horizPerp(n.z(), 0.f, -n.x());
            const float     hLen = horizPerp.len();
            constexpr float kMinHorizTangent = 1e-3f;   // degenerate (near-vertical) normal guard
            if (hLen > kMinHorizTangent) {
                c.tangent[0]   = horizPerp * (1.f / hLen);
                c.tangent[1]   = mu::Vec3{};
                c.tangentCount = 1;
            }
        }
        if (c.tangentCount == 2)
            buildTangentFrame(cp.normal, c.tangent);

        // Normal effective mass: 1 / (sum of inverse mass contributions).
        // Static bodies (invMass == 0) have infinite inertia, so their angular
        // contribution is excluded to avoid distorting effMass. Character bodies are
        // excluded for the same reason: applyImpulsePair() gives them no angular
        // impulse, so keeping the term here would make effMass disagree with the
        // impulse actually applied and the PGS rows would stop converging.
        const bool angularA = (bodyA->invMass() > 0.f) && !bodyA->isCharacterBody();
        const bool angularB = (bodyB->invMass() > 0.f) && !bodyB->isCharacterBody();

        const float angA_n = angularA
            ? angularEffectiveMass(c.rA, n, bodyA->invInertiaWorld()) : 0.f;
        const float angB_n = angularB
            ? angularEffectiveMass(c.rB, n, bodyB->invInertiaWorld()) : 0.f;
        c.effMassNormal = 1.f / (bodyA->invMass() + bodyB->invMass() + angA_n + angB_n);

        // Tangent effective masses.
        for (int t = 0; t < c.tangentCount; ++t) {
            const mu::Vec3 tv = c.tangent[t];
            const float angA_t = angularA
                ? angularEffectiveMass(c.rA, tv, bodyA->invInertiaWorld()) : 0.f;
            const float angB_t = angularB
                ? angularEffectiveMass(c.rB, tv, bodyB->invInertiaWorld()) : 0.f;
            c.effMassTangent[t] = 1.f / (bodyA->invMass() + bodyB->invMass() + angA_t + angB_t);
        }
        for (int t = c.tangentCount; t < 2; ++t)
            c.effMassTangent[t] = 0.f;

        // Dual penetration correction (velocity-level Baumgarte + position-level split impulse).
        // Terrain contacts skip Baumgarte to prevent ground vibration.
        const float penetration  = std::min(std::max(0.f, cp.depth - kSlop), kMaxCorrectionDepth);
        const float extVelProj   = mu::dot((externalAccelA_ - externalAccelB_) * dtf, n);
        const float extComp      = std::max(0.f, -extVelProj);
        const float baumgarteBeta = isTerrainContact_ ? 0.f : kBaumgarteBeta;
        // External-force (gravity) compensation is split across BOTH channels so each
        // is gravity-aware, while the total stays exactly extComp (no double-comp float).
        // The integrate-step sink and the combined push cancel: no creep, no float.
        const float extCompVel   = kExtCompVelFrac * extComp;
        const float extCompSplit = extComp - extCompVel;
        c.bias             = baumgarteBeta * invDt * penetration + extCompVel;
        c.pseudoBias       = kSplitImpulseBeta * invDt * penetration + extCompSplit;
        c.accNormalPseudo  = 0.f;
    }
}

// Apply an impulse j*dir to bodyA (+=) and -j*dir to bodyB (-=) at their
// respective contact radii, modifying both linear and angular velocity.
// Upright agents take contact impulses as pure translation. Rationale (measured):
// a horizontal push from an overlapping player lands on the boss's torso, far above
// its COM, so cross(r, J) * invInertiaWorld spins the body up. setMass() gives every
// body a fixed 0.5 m box inertia regardless of its real size, so that omega is wildly
// oversized for a large character. The spin then drives the FOOT contact points down
// into the terrain (relVel = linearVel + cross(omega, r)), the terrain constraint
// fights back with a large +Y impulse, and that impulse lifts the COM: the boss
// levitates while still looking upright (uprightStiffness snaps the tilt back, but
// the angular VELOCITY is what corrupts the contact solve).
// Characters get their facing from AI/network setOrient(), never from the solver, so
// dropping the angular channel loses nothing. Same rule as resolveStaticPenetration
// ("COM translation only - no rotation").
static void applyImpulsePair(RigidBody* a, RigidBody* b,
                              float j, mu::Vec3 dir,
                              mu::Vec3 rA, mu::Vec3 rB)
{
    const mu::Vec3 jDir = dir * j;

    // Guard on invMass: Static bodies (invMass==0) must not be moved.
    // Without this guard, a Static body's omega gets set via the identity
    // invInertiaWorld, corrupting subsequent relVel calculations in PGS.
    if (a->invMass() > 0.f) {
        a->setLinearVel(a->linearVel() + jDir * a->invMass());
        // In row-vector form: delta_omega = (r x J) * invInertiaWorld
        if (!a->isCharacterBody())
            a->setOmega(a->omega() + mu::cross(rA, jDir) * a->invInertiaWorld());
    }
    if (b->invMass() > 0.f) {
        b->setLinearVel(b->linearVel() - jDir * b->invMass());
        if (!b->isCharacterBody())
            b->setOmega(b->omega() - mu::cross(rB, jDir) * b->invInertiaWorld());
    }
}

static void applyPseudoImpulsePair(RigidBody* a, RigidBody* b,
                                    mu::Vec3 jDir,
                                    mu::Vec3 rA, mu::Vec3 rB)
{
    if (a->invMass() > 0.f) {
        a->addPseudoLinearVel( jDir * a->invMass());
        if (!a->isCharacterBody())
            a->addPseudoOmega( mu::cross(rA, jDir) * a->invInertiaWorld());
    }
    if (b->invMass() > 0.f) {
        b->addPseudoLinearVel(-jDir * b->invMass());
        if (!b->isCharacterBody())
            b->addPseudoOmega(-mu::cross(rB, jDir) * b->invInertiaWorld());
    }
}

void ContactConstraint::applyWarmStart()
{
    for (int i = 0; i < count; ++i) {
        const Cache&  c  = cache_[i];
        ContactPoint& cp = contacts[i];
        applyImpulsePair(bodyA, bodyB, cp.accNormal, mu::Vec3(cp.normal), c.rA, c.rB);
        for (int t = 0; t < c.tangentCount; ++t)
            applyImpulsePair(bodyA, bodyB, cp.accTangent[t], c.tangent[t], c.rA, c.rB);
        // PhysicsWorld::solveConstraints() seeds accTangent[0..1] from warmCache_
        // AFTER prepare() has run, so clearing the dead axes in prepare() would be
        // undone. Drop them here or a character contact keeps re-applying last
        // step's vertical friction impulse forever, defeating the single-axis rule.
        for (int t = c.tangentCount; t < 2; ++t)
            cp.accTangent[t] = 0.f;
    }
}

void ContactConstraint::solveVelocity()
{
    const float friction = std::sqrt(bodyA->friction() * bodyB->friction());

    for (int i = 0; i < count; ++i) {
        const Cache& c = cache_[i];
        ContactPoint& cp = contacts[i];

        const mu::Vec3 n  = cp.normal;
        const mu::Vec3 rA = c.rA;
        const mu::Vec3 rB = c.rB;

        // Relative velocity at contact point (A w.r.t. B).
        auto relVel = [&]() -> mu::Vec3 {
            return (bodyA->linearVel() + mu::cross(bodyA->omega(), rA))
                 - (bodyB->linearVel() + mu::cross(bodyB->omega(), rB));
        };

        // --- Normal impulse ---
        {
            const float Jv   = mu::dot(relVel(), n);
            float       jn   = -(Jv - c.bias) * c.effMassNormal;
            const float prev = cp.accNormal;
            cp.accNormal     = std::max(0.f, prev + jn);
            jn               = cp.accNormal - prev;

            applyImpulsePair(bodyA, bodyB, jn, n, rA, rB);
        }

        // --- Friction impulses (one horizontal axis for character contacts,
        //     otherwise the full tangent plane) ---
        const float maxFriction = friction * std::abs(cp.accNormal);
        for (int t = 0; t < c.tangentCount; ++t) {
            const mu::Vec3 tv = c.tangent[t];
            const float    Jv = mu::dot(relVel(), tv);
            float          jt = -Jv * c.effMassTangent[t];
            const float    prev = cp.accTangent[t];
            cp.accTangent[t] = std::clamp(prev + jt, -maxFriction, maxFriction);
            jt               = cp.accTangent[t] - prev;

            applyImpulsePair(bodyA, bodyB, jt, tv, rA, rB);
        }
    }
}

void ContactConstraint::solvePosition()
{
    for (int i = 0; i < count; ++i) {
        Cache& c = cache_[i];
        if (c.pseudoBias <= 0.f) continue;

        const ContactPoint& cp = contacts[i];
        const mu::Vec3 n = cp.normal;

        const mu::Vec3 pvA = bodyA->pseudoLinearVel()
                           + mu::cross(bodyA->pseudoOmega(), c.rA);
        const mu::Vec3 pvB = bodyB->pseudoLinearVel()
                           + mu::cross(bodyB->pseudoOmega(), c.rB);
        const float Jpv = mu::dot(pvA - pvB, n);

        float jpn = -(Jpv - c.pseudoBias) * c.effMassNormal;

        const float prev   = c.accNormalPseudo;
        c.accNormalPseudo  = std::max(0.f, prev + jpn);
        jpn                = c.accNormalPseudo - prev;

        applyPseudoImpulsePair(bodyA, bodyB, n * jpn, c.rA, c.rB);
    }
}
