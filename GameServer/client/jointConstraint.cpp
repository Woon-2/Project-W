#include "pch.hpp"
#include "jointConstraint.hpp"

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

// Angular effective mass for a pure angular constraint along axis d:
//   dot(d, d * invI)  [row-vector convention]
static float angEff1D(mu::Vec3 d, const mu::Mat3x3& invI)
{
    return mu::dot(d, d * invI);
}

// Build a tangent frame perpendicular to n  (same as contactConstraint.cpp).
static void buildPerp(mu::Vec3 n, mu::Vec3 out[2])
{
    const mu::Vec3 ref = (std::abs(n.x()) < 0.57735f)
                         ? mu::Vec3(1.f, 0.f, 0.f)
                         : mu::Vec3(0.f, 1.f, 0.f);
    out[0] = mu::Vec3(mu::normalize(mu::cross(ref, n)));
    out[1] = mu::Vec3(mu::normalize(mu::cross(n, out[0])));
}

// Build the 3x3 effective mass matrix K for a linear (translational) constraint
// at moment arms rA, rB.  Returns K^-1.
// If K is near-singular, returns a safe diagonal fallback.
static mu::Mat3x3 build3x3EffMassInv(RigidBody* a, mu::Vec3 rA,
                                      RigidBody* b, mu::Vec3 rB)
{
    const float mAB = a->invMass() + b->invMass();

    const mu::Vec3 ex(1.f, 0.f, 0.f);
    const mu::Vec3 ey(0.f, 1.f, 0.f);
    const mu::Vec3 ez(0.f, 0.f, 1.f);

    const mu::Vec3 rAxEx = mu::cross(rA, ex), rAxEy = mu::cross(rA, ey), rAxEz = mu::cross(rA, ez);
    const mu::Vec3 rBxEx = mu::cross(rB, ex), rBxEy = mu::cross(rB, ey), rBxEz = mu::cross(rB, ez);

    const mu::Mat3x3& iA = a->invInertiaWorld();
    const mu::Mat3x3& iB = b->invInertiaWorld();

    // K_ij = (invMA+invMB)*delta_ij
    //      + dot(rA x ei, (rA x ej)*iA)  +  dot(rB x ei, (rB x ej)*iB)
    auto Kij = [&](mu::Vec3 rAxEi, mu::Vec3 rAxEj,
                   mu::Vec3 rBxEi, mu::Vec3 rBxEj, float diag) {
        return diag + mu::dot(rAxEi, rAxEj * iA) + mu::dot(rBxEi, rBxEj * iB);
    };

    const float k00 = Kij(rAxEx, rAxEx, rBxEx, rBxEx, mAB);
    const float k11 = Kij(rAxEy, rAxEy, rBxEy, rBxEy, mAB);
    const float k22 = Kij(rAxEz, rAxEz, rBxEz, rBxEz, mAB);
    const float k01 = Kij(rAxEx, rAxEy, rBxEx, rBxEy, 0.f);
    const float k02 = Kij(rAxEx, rAxEz, rBxEx, rBxEz, 0.f);
    const float k12 = Kij(rAxEy, rAxEz, rBxEy, rBxEz, 0.f);

    mu::Mat3x3 K{};
    K.setRow(0, mu::Vec3(k00, k01, k02));
    K.setRow(1, mu::Vec3(k01, k11, k12));
    K.setRow(2, mu::Vec3(k02, k12, k22));

    const float det = k00 * (k11 * k22 - k12 * k12)
                    - k01 * (k01 * k22 - k12 * k02)
                    + k02 * (k01 * k12 - k11 * k02);
    if (std::abs(det) < 1e-10f) {
        // Near-singular: use diagonal fallback to prevent NaN.
        mu::Mat3x3 fallback{};
        const float d = (mAB > 1e-10f) ? (1.f / mAB) : 1.f;
        fallback.setRow(0, mu::Vec3(d, 0.f, 0.f));
        fallback.setRow(1, mu::Vec3(0.f, d, 0.f));
        fallback.setRow(2, mu::Vec3(0.f, 0.f, d));
        return fallback;
    }
    return mu::inverse(K);
}

// Apply a 3D impulse j to bodyA (+) and bodyB (-) at moment arms rA, rB.
static void applyLinearImpulsePair(RigidBody* a, RigidBody* b,
                                   mu::Vec3 j, mu::Vec3 rA, mu::Vec3 rB)
{
    if (a->invMass() > 0.f) {
        a->setLinearVel(a->linearVel() + j * a->invMass());
        a->setOmega(a->omega() + mu::cross(rA, j) * a->invInertiaWorld());
    }
    if (b->invMass() > 0.f) {
        b->setLinearVel(b->linearVel() - j * b->invMass());
        b->setOmega(b->omega() - mu::cross(rB, j) * b->invInertiaWorld());
    }
}

// Apply a scalar angular impulse j*axis to bodyA (+) and bodyB (-).
static void applyAngularImpulsePair(RigidBody* a, RigidBody* b,
                                    float j, mu::Vec3 axis)
{
    a->applyTorqueImpulse(axis * j);
    b->applyTorqueImpulse(axis * -j);
}

// Relative velocity at contact points defined by moment arms rA, rB.
static mu::Vec3 relVelAt(const RigidBody* a, const RigidBody* b,
                         mu::Vec3 rA, mu::Vec3 rB)
{
    return (a->linearVel() + mu::cross(a->omega(), rA))
         - (b->linearVel() + mu::cross(b->omega(), rB));
}

// Multiply two NQuats via Quat intermediary and return normalised NQuat.
// DirectXMath convention: XMQuaternionMultiply(Q1, Q2) = Q1*Q2.
static mu::NQuat mulQ(mu::NQuat a, mu::NQuat b)
{
    return mu::NQuat(mu::Quat(a) * mu::Quat(b));
}

// Decompose q = swing * twist  where twist is a rotation around twistAxis.
static void swingTwistDecompose(mu::NQuat q, mu::Vec3 twistAxis,
                                mu::NQuat& outSwing, mu::NQuat& outTwist)
{
    const mu::Vec3 qxyz(q.x(), q.y(), q.z());
    const mu::Vec3 proj = twistAxis * mu::dot(qxyz, twistAxis);
    const float twistLen2 = proj.len2() + q.w() * q.w();

    if (twistLen2 > 1e-10f) {
        // Normalise directly via XMVECTOR to avoid extra sqrt.
        const mu::Quat twistRaw(proj.x(), proj.y(), proj.z(), q.w());
        outTwist = mu::NQuat(twistRaw);
    } else {
        outTwist = mu::NQuat{};
    }
    // swing = q * twist^-1
    outSwing = mulQ(q, ~outTwist);
}

// ---------------------------------------------------------------------------
// BallSocketJoint
// ---------------------------------------------------------------------------

BallSocketJoint::BallSocketJoint(RigidBody* a, RigidBody* b,
                                 mu::Vec3 anchorA, mu::Vec3 anchorB)
    : bodyA_(a), bodyB_(b), anchorA_(anchorA), anchorB_(anchorB)
{}

void BallSocketJoint::prepare(Seconds dt)
{
    const float dtf   = dt.count();
    const float invDt = (dtf > 0.f) ? (1.f / dtf) : 0.f;

    cache_.rA = bodyA_->orient().rotate(anchorA_);
    cache_.rB = bodyB_->orient().rotate(anchorB_);

    const mu::Vec3 posError = (bodyA_->pos() + cache_.rA)
                            - (bodyB_->pos() + cache_.rB);
    cache_.effMassInv = build3x3EffMassInv(bodyA_, cache_.rA, bodyB_, cache_.rB);
    cache_.bias       = posError * (kJointBeta * invDt);

    // Warm-start: re-apply accumulated impulse.
    applyLinearImpulsePair(bodyA_, bodyB_, cache_.accImpulse, cache_.rA, cache_.rB);
}

void BallSocketJoint::solveVelocity()
{
    const mu::Vec3 rv      = relVelAt(bodyA_, bodyB_, cache_.rA, cache_.rB);
    const mu::Vec3 cVel    = rv + cache_.bias;
    const mu::Vec3 dLambda = -(cVel * cache_.effMassInv);  // row-vector: v * M^-1

    cache_.accImpulse = cache_.accImpulse + dLambda;       // bilateral: no clamp
    applyLinearImpulsePair(bodyA_, bodyB_, dLambda, cache_.rA, cache_.rB);
}

// ---------------------------------------------------------------------------
// HingeJoint
// ---------------------------------------------------------------------------

HingeJoint::HingeJoint(RigidBody* a, RigidBody* b,
                       mu::Vec3 anchorA, mu::Vec3 anchorB,
                       mu::Vec3 axisA, float minAngle, float maxAngle)
    : bodyA_(a), bodyB_(b)
    , anchorA_(anchorA), anchorB_(anchorB)
    , axisA_(mu::Vec3(mu::normalize(axisA)))
    , minAngle_(minAngle), maxAngle_(maxAngle)
{
    // Reference relative orientation: q_ref = conj(bodyA) * bodyB at build time.
    refOrient_ = mulQ(~bodyA_->orient(), bodyB_->orient());
}

void HingeJoint::prepare(Seconds dt)
{
    const float dtf   = dt.count();
    const float invDt = (dtf > 0.f) ? (1.f / dtf) : 0.f;

    // --- Translational ---
    cache_.rA = bodyA_->orient().rotate(anchorA_);
    cache_.rB = bodyB_->orient().rotate(anchorB_);

    const mu::Vec3 posError = (bodyA_->pos() + cache_.rA)
                            - (bodyB_->pos() + cache_.rB);
    cache_.linEffMassInv = build3x3EffMassInv(bodyA_, cache_.rA, bodyB_, cache_.rB);
    cache_.linBias       = posError * (kJointBeta * invDt);

    // --- Angular alignment ---
    // hingeAxis in world space (derived from bodyA orientation).
    cache_.hingeAxisWorld = mu::Vec3(mu::normalize(bodyA_->orient().rotate(axisA_)));
    buildPerp(cache_.hingeAxisWorld, cache_.perpAxes);

    // World-space axis of bodyB — should align with hingeAxisWorld.
    const mu::Vec3 axisBWorld = mu::Vec3(mu::normalize(bodyB_->orient().rotate(axisA_)));

    for (int i = 0; i < 2; ++i) {
        const mu::Vec3& perp = cache_.perpAxes[i];
        const float contribA = (bodyA_->invMass() > 0.f)
            ? angEff1D(perp, bodyA_->invInertiaWorld()) : 0.f;
        const float contribB = (bodyB_->invMass() > 0.f)
            ? angEff1D(perp, bodyB_->invInertiaWorld()) : 0.f;
        cache_.angEffMass[i] = 1.f / (contribA + contribB + 1e-10f);

        // Angular velocity bias: drive axisB toward axisA.
        const float angViol = mu::dot(axisBWorld, perp);
        cache_.angBias[i] = angViol * (kJointBeta * invDt);
    }

    // --- Angle limit ---
    cache_.limitActive = false;
    if (minAngle_ < maxAngle_) {
        // q_delta = refOrient^-1 * (conj(bodyA) * bodyB)
        const mu::NQuat relOrient = mulQ(~bodyA_->orient(), bodyB_->orient());
        const mu::NQuat qDelta    = mulQ(~refOrient_, relOrient);

        mu::NQuat swing, twist;
        swingTwistDecompose(qDelta, axisA_, swing, twist);

        float tw = std::clamp(twist.w(), -1.f, 1.f);
        const float twistAngle = 2.f * std::acos(std::abs(tw));
        // Sign: positive if twist xyz aligns with axisA_.
        const mu::Vec3 twistXYZ(twist.x(), twist.y(), twist.z());
        const float sign = (mu::dot(twistXYZ, axisA_) >= 0.f) ? 1.f : -1.f;
        const float currentAngle = sign * twistAngle;

        const float contribA = (bodyA_->invMass() > 0.f)
            ? angEff1D(cache_.hingeAxisWorld, bodyA_->invInertiaWorld()) : 0.f;
        const float contribB = (bodyB_->invMass() > 0.f)
            ? angEff1D(cache_.hingeAxisWorld, bodyB_->invInertiaWorld()) : 0.f;
        cache_.limitEffMass = 1.f / (contribA + contribB + 1e-10f);

        if (currentAngle < minAngle_) {
            cache_.limitActive = true;
            cache_.limitLo     = true;
            cache_.limitBias   = (minAngle_ - currentAngle) * (kJointBeta * invDt);
        } else if (currentAngle > maxAngle_) {
            cache_.limitActive = true;
            cache_.limitLo     = false;
            cache_.limitBias   = (currentAngle - maxAngle_) * (kJointBeta * invDt);
        }
        if (!cache_.limitActive)
            cache_.limitAccImp = 0.f;
    }

    // Warm-start.
    applyLinearImpulsePair(bodyA_, bodyB_, cache_.linAccImp, cache_.rA, cache_.rB);
    for (int i = 0; i < 2; ++i)
        applyAngularImpulsePair(bodyA_, bodyB_, cache_.angAccImp[i], cache_.perpAxes[i]);
    if (cache_.limitActive)
        applyAngularImpulsePair(bodyA_, bodyB_, cache_.limitAccImp, cache_.hingeAxisWorld);
}

void HingeJoint::solveVelocity()
{
    // --- Translational ---
    {
        const mu::Vec3 rv      = relVelAt(bodyA_, bodyB_, cache_.rA, cache_.rB);
        const mu::Vec3 cVel    = rv + cache_.linBias;
        const mu::Vec3 dLambda = -(cVel * cache_.linEffMassInv);
        cache_.linAccImp = cache_.linAccImp + dLambda;
        applyLinearImpulsePair(bodyA_, bodyB_, dLambda, cache_.rA, cache_.rB);
    }

    // --- Angular alignment ---
    const mu::Vec3 relOmega = bodyA_->omega() - bodyB_->omega();
    for (int i = 0; i < 2; ++i) {
        const mu::Vec3& perp = cache_.perpAxes[i];
        const float Jv       = mu::dot(relOmega, perp);
        const float dL       = -(Jv + cache_.angBias[i]) * cache_.angEffMass[i];
        const float prev     = cache_.angAccImp[i];
        cache_.angAccImp[i] += dL;  // bilateral, no clamp
        applyAngularImpulsePair(bodyA_, bodyB_, cache_.angAccImp[i] - prev, perp);
    }

    // --- Limit ---
    if (cache_.limitActive) {
        const float Jv   = mu::dot(relOmega, cache_.hingeAxisWorld);
        const float dL   = -(Jv + cache_.limitBias) * cache_.limitEffMass;
        const float prev = cache_.limitAccImp;
        cache_.limitAccImp = cache_.limitLo
            ? std::max(0.f, prev + dL)
            : std::min(0.f, prev + dL);
        applyAngularImpulsePair(bodyA_, bodyB_, cache_.limitAccImp - prev, cache_.hingeAxisWorld);
    }
}

// ---------------------------------------------------------------------------
// ConeTwistJoint
// ---------------------------------------------------------------------------

ConeTwistJoint::ConeTwistJoint(RigidBody* a, RigidBody* b,
                               mu::Vec3 anchorA, mu::Vec3 anchorB,
                               mu::NQuat refOrientA, mu::NQuat refOrientB,
                               float coneHalfAngle, float twistLimit)
    : bodyA_(a), bodyB_(b)
    , anchorA_(anchorA), anchorB_(anchorB)
    , refOrientA_(refOrientA), refOrientB_(refOrientB)
    , coneHalfAngle_(std::min(coneHalfAngle, mu::pi * 0.85f))
    , twistLimit_(std::abs(twistLimit))
{}

void ConeTwistJoint::prepare(Seconds dt)
{
    const float dtf   = dt.count();
    const float invDt = (dtf > 0.f) ? (1.f / dtf) : 0.f;

    // --- Translational ---
    cache_.rA = bodyA_->orient().rotate(anchorA_);
    cache_.rB = bodyB_->orient().rotate(anchorB_);

    const mu::Vec3 posError = (bodyA_->pos() + cache_.rA)
                            - (bodyB_->pos() + cache_.rB);
    cache_.linEffMassInv = build3x3EffMassInv(bodyA_, cache_.rA, bodyB_, cache_.rB);
    cache_.linBias       = posError * (kJointBeta * invDt);

    // --- Joint-space relative orientation ---
    // q_jointA = refOrientA^-1 * bodyA.orient  (current bodyA orientation in joint space)
    const mu::NQuat qJointA = mulQ(~refOrientA_, bodyA_->orient());
    const mu::NQuat qJointB = mulQ(~refOrientB_, bodyB_->orient());
    // q_rel = qJointA^-1 * qJointB  (relative rotation in joint space)
    const mu::NQuat qRel    = mulQ(~qJointA, qJointB);

    // Twist axis is +Z in joint space, brought to world space via bodyA orientation.
    const mu::Vec3 twistAxisJoint(0.f, 0.f, 1.f);
    const mu::Vec3 twistAxisWorld = mu::Vec3(mu::normalize(bodyA_->orient().rotate(twistAxisJoint)));

    // Decompose relative rotation into swing + twist around the joint Z axis.
    mu::NQuat swing, twist;
    swingTwistDecompose(qRel, twistAxisJoint, swing, twist);

    // --- Cone violation ---
    cache_.coneActive = false;
    {
        const float sw = std::clamp(swing.w(), -1.f, 1.f);
        const float swingAngle = 2.f * std::acos(std::abs(sw));

        if (swingAngle > coneHalfAngle_) {
            cache_.coneActive = true;
            const float viol  = swingAngle - coneHalfAngle_;

            // Correction axis: swing rotation axis in world space.
            const mu::Vec3 swingXYZ(swing.x(), swing.y(), swing.z());
            const float swLen = std::sqrt(swingXYZ.len2());
            cache_.coneAxis = (swLen > 1e-6f)
                ? mu::Vec3(mu::normalize(bodyA_->orient().rotate(swingXYZ / swLen)))
                : twistAxisWorld;

            const float cA = (bodyA_->invMass() > 0.f)
                ? angEff1D(cache_.coneAxis, bodyA_->invInertiaWorld()) : 0.f;
            const float cB = (bodyB_->invMass() > 0.f)
                ? angEff1D(cache_.coneAxis, bodyB_->invInertiaWorld()) : 0.f;
            cache_.coneEffMass = 1.f / (cA + cB + 1e-10f);
            cache_.coneBias    = viol * (kJointBeta * invDt);
        } else {
            cache_.coneAccImp = 0.f;
        }
    }

    // --- Twist violation ---
    cache_.twistActive = false;
    {
        const float tw = std::clamp(twist.w(), -1.f, 1.f);
        const float twistAngle = 2.f * std::acos(std::abs(tw));
        const float sign = (twist.z() >= 0.f) ? 1.f : -1.f;
        const float signedTwist = sign * twistAngle;

        if (signedTwist < -twistLimit_ || signedTwist > twistLimit_) {
            cache_.twistActive = true;
            cache_.twistLo     = (signedTwist < -twistLimit_);
            const float viol   = cache_.twistLo
                ? (-twistLimit_ - signedTwist)
                : (signedTwist  - twistLimit_);

            cache_.twistAxis = twistAxisWorld;

            const float cA = (bodyA_->invMass() > 0.f)
                ? angEff1D(twistAxisWorld, bodyA_->invInertiaWorld()) : 0.f;
            const float cB = (bodyB_->invMass() > 0.f)
                ? angEff1D(twistAxisWorld, bodyB_->invInertiaWorld()) : 0.f;
            cache_.twistEffMass = 1.f / (cA + cB + 1e-10f);
            cache_.twistBias    = viol * (kJointBeta * invDt);
        } else {
            cache_.twistAccImp = 0.f;
        }
    }

    // Warm-start.
    applyLinearImpulsePair(bodyA_, bodyB_, cache_.linAccImp, cache_.rA, cache_.rB);
    if (cache_.coneActive)
        applyAngularImpulsePair(bodyA_, bodyB_, cache_.coneAccImp, cache_.coneAxis);
    if (cache_.twistActive)
        applyAngularImpulsePair(bodyA_, bodyB_, cache_.twistAccImp, cache_.twistAxis);
}

void ConeTwistJoint::solveVelocity()
{
    // --- Translational ---
    {
        const mu::Vec3 rv      = relVelAt(bodyA_, bodyB_, cache_.rA, cache_.rB);
        const mu::Vec3 cVel    = rv + cache_.linBias;
        const mu::Vec3 dLambda = -(cVel * cache_.linEffMassInv);
        cache_.linAccImp = cache_.linAccImp + dLambda;
        applyLinearImpulsePair(bodyA_, bodyB_, dLambda, cache_.rA, cache_.rB);
    }

    const mu::Vec3 relOmega = bodyA_->omega() - bodyB_->omega();

    // --- Cone ---
    if (cache_.coneActive) {
        const float Jv   = mu::dot(relOmega, cache_.coneAxis);
        const float dL   = -(Jv + cache_.coneBias) * cache_.coneEffMass;
        const float prev = cache_.coneAccImp;
        cache_.coneAccImp = std::max(0.f, prev + dL);  // one-sided
        applyAngularImpulsePair(bodyA_, bodyB_, cache_.coneAccImp - prev, cache_.coneAxis);
    }

    // --- Twist ---
    if (cache_.twistActive) {
        const float Jv   = mu::dot(relOmega, cache_.twistAxis);
        const float dL   = -(Jv + cache_.twistBias) * cache_.twistEffMass;
        const float prev = cache_.twistAccImp;
        cache_.twistAccImp = cache_.twistLo
            ? std::max(0.f, prev + dL)
            : std::min(0.f, prev + dL);
        applyAngularImpulsePair(bodyA_, bodyB_, cache_.twistAccImp - prev, cache_.twistAxis);
    }
}
