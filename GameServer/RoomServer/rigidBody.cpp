#include "rspch.hpp"
#include "rigidBody.hpp"

// Build a 3x3 diagonal matrix from three diagonal values.
static mu::Mat3x3 diagMat3(float d0, float d1, float d2)
{
    mu::Mat3x3 m{};
    m.setRow(0, mu::Vec3(d0, 0.f, 0.f));
    m.setRow(1, mu::Vec3(0.f, d1, 0.f));
    m.setRow(2, mu::Vec3(0.f, 0.f, d2));
    return m;
}

mu::Mat3x3 computeBoxInertia(float mass, mu::Vec3 halfExtents)
{
    const float hx = halfExtents.x(), hy = halfExtents.y(), hz = halfExtents.z();
    return diagMat3(
        (1.f / 3.f) * mass * (hy * hy + hz * hz),
        (1.f / 3.f) * mass * (hx * hx + hz * hz),
        (1.f / 3.f) * mass * (hx * hx + hy * hy)
    );
}

mu::Mat3x3 computeCapsuleInertia(float mass, float radius, float halfHeight)
{
    const float r2  = radius * radius;
    const float h   = 2.f * halfHeight;
    const float kxz = (1.f / 12.f) * mass * (3.f * r2 + h * h);
    return diagMat3(kxz, 0.5f * mass * r2, kxz);
}

// ---------------------------------------------------------------------------

void RigidBody::setMass(float mass)
{
    invMass_ = (mass > 0.f) ? (1.f / mass) : 0.f;
    const auto defaultI = computeBoxInertia(mass, mu::Vec3{ 0.5f, 0.5f, 0.5f });
    invInertiaLocal_ = mu::inverse(defaultI);
    updateInertiaWorld();
}

void RigidBody::setInertia(const mu::Mat3x3& localInertia)
{
    invInertiaLocal_ = mu::inverse(localInertia);
    updateInertiaWorld();
}

void RigidBody::updateInertiaWorld()
{
    const mu::Mat3x3 M  = mu::Mat3x3(curr_.orient);
    const mu::Mat3x3 Mt = mu::transpose(M);
    invInertiaWorld_ = Mt * invInertiaLocal_ * M;
}

void RigidBody::applyForce(mu::Vec3 f)
{
    forceAccum_ = forceAccum_ + f;
}

void RigidBody::applyForceAtPoint(mu::Vec3 f, mu::Vec3 worldPoint)
{
    forceAccum_  = forceAccum_  + f;
    torqueAccum_ = torqueAccum_ + mu::cross(worldPoint - curr_.pos, f);
}

void RigidBody::applyImpulse(mu::Vec3 j, mu::Vec3 worldPoint)
{
    if (invMass_ == 0.f) return;
    curr_.linearVel = curr_.linearVel + j * invMass_;
    const auto angularImpulse = mu::cross(worldPoint - curr_.pos, j);
    curr_.omega = curr_.omega + angularImpulse * invInertiaWorld_;
}

void RigidBody::clearAccumulators()
{
    forceAccum_  = mu::Vec3{};
    torqueAccum_ = mu::Vec3{};
}
