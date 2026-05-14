#include "pch.hpp"
#include "constraint.hpp"

float solveJacobianRow(const JacobianRow& row, float& accImpulse, float sor)
{
    auto linVel = [&](const RigidBody* b) {
        return row.pseudo ? b->pseudoLinearVel() : b->linearVel();
    };
    auto angVel = [&](const RigidBody* b) {
        return row.pseudo ? b->pseudoOmega() : b->omega();
    };

    const float Jv =
        mu::dot(row.linA, linVel(row.bodyA)) + mu::dot(row.angA, angVel(row.bodyA)) +
        mu::dot(row.linB, linVel(row.bodyB)) + mu::dot(row.angB, angVel(row.bodyB));

    const float rawDelta = -(Jv + row.rhs) * row.effMass;
    const float prev = accImpulse;
    const float candidate = std::clamp(prev + rawDelta, row.lower, row.upper);
    const float delta = (candidate - prev) * sor;
    accImpulse = prev + delta;
    if (delta == 0.f)
        return 0.f;

    if (row.bodyA->invMass() > 0.f) {
        const mu::Vec3 dv = row.linA * (delta * row.bodyA->invMass());
        const mu::Vec3 dw = (row.angA * delta) * row.bodyA->invInertiaWorld();
        if (row.pseudo) {
            row.bodyA->addPseudoLinearVel(dv);
            row.bodyA->addPseudoOmega(dw);
        } else {
            row.bodyA->setLinearVel(row.bodyA->linearVel() + dv);
            row.bodyA->setOmega(row.bodyA->omega() + dw);
        }
    }

    if (row.bodyB->invMass() > 0.f) {
        const mu::Vec3 dv = row.linB * (delta * row.bodyB->invMass());
        const mu::Vec3 dw = (row.angB * delta) * row.bodyB->invInertiaWorld();
        if (row.pseudo) {
            row.bodyB->addPseudoLinearVel(dv);
            row.bodyB->addPseudoOmega(dw);
        } else {
            row.bodyB->setLinearVel(row.bodyB->linearVel() + dv);
            row.bodyB->setOmega(row.bodyB->omega() + dw);
        }
    }

    return delta;
}