#include "pch.hpp"
#include "rigidBody.hpp"

// Build a 3x3 diagonal matrix from three diagonal values.
static mu::Mat3x3 diagMat3(float d0, float d1, float d2)
{
    // Default-construct to identity so row 3 = (0, 0, 0, 1).
    // Mat(0.f) would zero the entire 4x4 matrix, making it singular
    // and causing XMMatrixInverse to return NaN.
    mu::Mat3x3 m{};
    m.setRow(0, mu::Vec3(d0, 0.f, 0.f));
    m.setRow(1, mu::Vec3(0.f, d1, 0.f));
    m.setRow(2, mu::Vec3(0.f, 0.f, d2));
    return m;
}

// Build a diagonal inertia tensor for a solid box of full size (2*h).
// Standard formula: I_x = (1/3)*m*(hy^2 + hz^2), etc.
mu::Mat3x3 computeBoxInertia(float mass, mu::Vec3 halfExtents)
{
    const float hx = halfExtents.x(), hy = halfExtents.y(), hz = halfExtents.z();
    return diagMat3(
        (1.f / 3.f) * mass * (hy * hy + hz * hz),
        (1.f / 3.f) * mass * (hx * hx + hz * hz),
        (1.f / 3.f) * mass * (hx * hx + hy * hy)
    );
}

// Approximate inertia tensor for a capsule aligned with the Y axis.
// Treated as a cylinder: I_y = 0.5*m*r^2, I_x = I_z = (1/12)*m*(3r^2+h^2).
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
    // Default inertia: unit half-extent box scaled by actual mass.
    // Callers may override by calling setInertia() afterwards.
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
    if (rotationLocked_) return;
    const mu::Mat3x3 M  = mu::Mat3x3(curr_.orient);
    const mu::Mat3x3 Mt = mu::transpose(M);
    invInertiaWorld_ = Mt * invInertiaLocal_ * M;
}

void RigidBody::lockRotation()
{
    rotationLocked_ = true;
    // Zero all rows to make invInertia a zero matrix (infinite rotational inertia).
    mu::Mat3x3 zero{};
    zero.setRow(0, mu::Vec3(0.f, 0.f, 0.f));
    zero.setRow(1, mu::Vec3(0.f, 0.f, 0.f));
    zero.setRow(2, mu::Vec3(0.f, 0.f, 0.f));
    invInertiaLocal_ = zero;
    invInertiaWorld_ = zero;
    curr_.omega = mu::Vec3{};
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
    // alpha = I_world^-1 * tau  →  in row-vector form: tau * invInertiaWorld
    curr_.omega = curr_.omega + angularImpulse * invInertiaWorld_;
}

void RigidBody::applyTorqueImpulse(mu::Vec3 tau)
{
    if (invMass_ == 0.f) return;
    curr_.omega = curr_.omega + tau * invInertiaWorld_;
}

void RigidBody::clearAccumulators()
{
    forceAccum_  = mu::Vec3{};
    torqueAccum_ = mu::Vec3{};
}
