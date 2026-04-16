#include "rspch.hpp"
#include "contactConstraint.hpp"

ContactConstraint::ContactConstraint(RigidBody* a, RigidBody* b)
    : bodyA(a), bodyB(b)
{}

void ContactConstraint::addContact(const ContactPoint& cp)
{
    if (count < 4)
        contacts[count++] = cp;
}

static void buildTangentFrame(mu::NVec3 normal, mu::Vec3 outTangent[2])
{
    const mu::Vec3 n = normal;
    mu::Vec3 ref = (std::abs(n.x()) < 0.57735f)
                   ? mu::Vec3(1.f, 0.f, 0.f)
                   : mu::Vec3(0.f, 1.f, 0.f);

    outTangent[0] = mu::Vec3(mu::normalize(mu::cross(ref, n)));
    outTangent[1] = mu::Vec3(mu::normalize(mu::cross(n, outTangent[0])));
}

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

        buildTangentFrame(cp.normal, c.tangent);

        const mu::Vec3 n = cp.normal;

        const float angA_n = (bodyA->invMass() > 0.f)
            ? angularEffectiveMass(c.rA, n, bodyA->invInertiaWorld()) : 0.f;
        const float angB_n = (bodyB->invMass() > 0.f)
            ? angularEffectiveMass(c.rB, n, bodyB->invInertiaWorld()) : 0.f;
        c.effMassNormal = 1.f / (bodyA->invMass() + bodyB->invMass() + angA_n + angB_n);

        for (int t = 0; t < 2; ++t) {
            const mu::Vec3 tv = c.tangent[t];
            const float angA_t = (bodyA->invMass() > 0.f)
                ? angularEffectiveMass(c.rA, tv, bodyA->invInertiaWorld()) : 0.f;
            const float angB_t = (bodyB->invMass() > 0.f)
                ? angularEffectiveMass(c.rB, tv, bodyB->invInertiaWorld()) : 0.f;
            c.effMassTangent[t] = 1.f / (bodyA->invMass() + bodyB->invMass() + angA_t + angB_t);
        }

        const float penetration = std::max(0.f, cp.depth - kSlop);
        c.bias = kBaumgarteBeta * invDt * penetration;
    }
}

static void applyImpulsePair(RigidBody* a, RigidBody* b,
                              float j, mu::Vec3 dir,
                              mu::Vec3 rA, mu::Vec3 rB)
{
    const mu::Vec3 jDir = dir * j;

    if (a->invMass() > 0.f) {
        a->setLinearVel(a->linearVel() + jDir * a->invMass());
        a->setOmega(a->omega() + mu::cross(rA, jDir) * a->invInertiaWorld());
    }
    if (b->invMass() > 0.f) {
        b->setLinearVel(b->linearVel() - jDir * b->invMass());
        b->setOmega(b->omega() - mu::cross(rB, jDir) * b->invInertiaWorld());
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

        auto relVel = [&]() -> mu::Vec3 {
            return (bodyA->linearVel() + mu::cross(bodyA->omega(), rA))
                 - (bodyB->linearVel() + mu::cross(bodyB->omega(), rB));
        };

        // Normal impulse
        {
            const float Jv   = mu::dot(relVel(), n);
            float       jn   = -(Jv - c.bias) * c.effMassNormal;
            const float prev = cp.accNormal;
            cp.accNormal     = std::max(0.f, prev + jn);
            jn               = cp.accNormal - prev;

            applyImpulsePair(bodyA, bodyB, jn, n, rA, rB);
        }

        // Friction impulses
        const float maxFriction = friction * std::abs(cp.accNormal);
        for (int t = 0; t < 2; ++t) {
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
