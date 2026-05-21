#include "pch.hpp"
#include "jointConstraint.hpp"

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

// Under-relaxation factor for angular constraint impulses.
// Branching joint structures (e.g. Chest with 3 child joints) can create
// PGS interference: each joint independently over-corrects the shared body.
// Scaling the per-iteration delta by kAngSOR prevents overshoot without
// needing more iterations.  Position-level solveJacobianRow calls are
// unaffected (they use the default sor=1.f).
static constexpr float kAngSOR = 0.6f;

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

    float k00 = Kij(rAxEx, rAxEx, rBxEx, rBxEx, mAB);
    float k11 = Kij(rAxEy, rAxEy, rBxEy, rBxEy, mAB);
    float k22 = Kij(rAxEz, rAxEz, rBxEz, rBxEz, mAB);
    float k01 = Kij(rAxEx, rAxEy, rBxEx, rBxEy, 0.f);
    float k02 = Kij(rAxEx, rAxEz, rBxEx, rBxEz, 0.f);
    float k12 = Kij(rAxEy, rAxEz, rBxEy, rBxEz, 0.f);

    // CFM
    k00 += Constraint::linearCFM;
    k11 += Constraint::linearCFM;
    k22 += Constraint::linearCFM;

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

static void applyPseudoLinearImpulsePair(RigidBody* a, RigidBody* b,
                                         mu::Vec3 j, mu::Vec3 rA, mu::Vec3 rB)
{
    if (a->invMass() > 0.f) {
        a->addPseudoLinearVel(j * a->invMass());
        a->addPseudoOmega(mu::cross(rA, j) * a->invInertiaWorld());
    }
    if (b->invMass() > 0.f) {
        b->addPseudoLinearVel(-j * b->invMass());
        b->addPseudoOmega(-mu::cross(rB, j) * b->invInertiaWorld());
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

static mu::Vec3 pseudoRelVelAt(const RigidBody* a, const RigidBody* b,
                               mu::Vec3 rA, mu::Vec3 rB)
{
    return (a->pseudoLinearVel() + mu::cross(a->pseudoOmega(), rA))
         - (b->pseudoLinearVel() + mu::cross(b->pseudoOmega(), rB));
}

// Multiply two NQuats via Quat intermediary and return normalised NQuat.
// DirectXMath convention: XMQuaternionMultiply(Q1, Q2) = Q1*Q2.
static mu::NQuat mulQ(mu::NQuat a, mu::NQuat b)
{
    return mu::NQuat(mu::Quat(a) * mu::Quat(b));
}

static float clampAbs(float v, float maxAbs)
{
    return std::clamp(v, -maxAbs, maxAbs);
}

// Decompose q = swing * twist  where twist is a rotation around twistAxis.
static void swingTwistDecompose(mu::NQuat q, mu::Vec3 twistAxis,
                                mu::NQuat& outSwing, mu::NQuat& outTwist)
{
    // Canonicalise: q and -q represent the same rotation. Ensure w >= 0 so
    // that outTwist and outSwing come out in canonical form, preventing
    // sign-flip artefacts in the angle/axis extraction in prepare().
    const float cs = (q.w() >= 0.f) ? 1.f : -1.f;
    const float qx = q.x() * cs, qy = q.y() * cs, qz = q.z() * cs, qw = q.w() * cs;

    const mu::Vec3 qxyz(qx, qy, qz);
    const mu::Vec3 proj = twistAxis * mu::dot(qxyz, twistAxis);
    const float twistLen2 = proj.len2() + qw * qw;

    if (twistLen2 > 1e-10f) {
        const mu::Quat twistRaw(proj.x(), proj.y(), proj.z(), qw);
        outTwist = mu::NQuat(twistRaw);
    } else {
        outTwist = mu::NQuat{};
    }
    // swing = q_canon * twist^-1
    outSwing = mulQ(mu::NQuat(mu::Quat(qx, qy, qz, qw)), ~outTwist);
}

// ---------------------------------------------------------------------------
// BallSocketJoint
// ---------------------------------------------------------------------------

BallSocketJoint::BallSocketJoint(RigidBody* a, RigidBody* b,
                                 mu::Vec3 anchorA, mu::Vec3 anchorB)
    : bodyA_(a), bodyB_(b), anchorA_(anchorA), anchorB_(anchorB), damping_(0.1f)
{}

void BallSocketJoint::resetAnchors()
{
    const mu::Vec3 pivotWorld = bodyB_->pos() + bodyB_->orient().rotate(anchorB_);
    anchorA_ = (~bodyA_->orient()).rotate(pivotWorld - bodyA_->pos());
}

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
    cache_.pseudoBias = posError * (kSplitBeta * invDt);
    cache_.pseudoAccImpulse = {};

    // Warm-start: re-apply accumulated impulse.
    applyLinearImpulsePair(bodyA_, bodyB_, cache_.accImpulse, cache_.rA, cache_.rB);
}

void BallSocketJoint::solveVelocity()
{
    // damping
    const mu::Vec3 rv = relVelAt(bodyA_, bodyB_, cache_.rA, cache_.rB);
    mu::Vec3 dampingImp = -(rv * cache_.effMassInv) * damping_;
    applyLinearImpulsePair(bodyA_, bodyB_, dampingImp, cache_.rA, cache_.rB);

    // impulse
    const mu::Vec3 cVel    = rv + cache_.bias;
    const mu::Vec3 dLambda = -(cVel * cache_.effMassInv);  // row-vector: v * M^-1

    cache_.accImpulse = cache_.accImpulse + dLambda;       // bilateral: no clamp
    applyLinearImpulsePair(bodyA_, bodyB_, dLambda, cache_.rA, cache_.rB);
}

void BallSocketJoint::solvePosition()
{
    const mu::Vec3 rv      = pseudoRelVelAt(bodyA_, bodyB_, cache_.rA, cache_.rB);
    const mu::Vec3 cVel    = rv + cache_.pseudoBias;
    const mu::Vec3 dLambda = -(cVel * cache_.effMassInv);

    cache_.pseudoAccImpulse = cache_.pseudoAccImpulse + dLambda;
    applyPseudoLinearImpulsePair(bodyA_, bodyB_, dLambda, cache_.rA, cache_.rB);
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
    , axisB_(mu::Vec3(mu::normalize((~b->orient()).rotate(a->orient().rotate(axisA_)))))
    , minAngle_(minAngle), maxAngle_(maxAngle), linearDamping_(0.1f), angularDamping_(0.1f)
{
    // Reference relative orientation: q_ref = conj(bodyA) * bodyB at build time.
    refOrient_ = mulQ(~bodyA_->orient(), bodyB_->orient());
}

void HingeJoint::resetAnchors()
{
    const mu::Vec3 pivotWorld = bodyB_->pos() + bodyB_->orient().rotate(anchorB_);
    anchorA_ = (~bodyA_->orient()).rotate(pivotWorld - bodyA_->pos());
    axisB_ = mu::Vec3(mu::normalize((~bodyB_->orient()).rotate(bodyA_->orient().rotate(axisA_))));
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
    cache_.linPseudoBias = posError * (kJointBeta * 3.f * invDt);
    cache_.linPseudoAccImp = {};

    // --- Angular alignment ---
    // hingeAxis in world space (derived from bodyA orientation).
    cache_.hingeAxisWorld = mu::Vec3(mu::normalize(bodyA_->orient().rotate(axisA_)));
    buildPerp(cache_.hingeAxisWorld, cache_.perpAxes);

    // World-space axis of bodyB — should align with hingeAxisWorld.
    const mu::Vec3 axisBWorld = mu::Vec3(mu::normalize(bodyB_->orient().rotate(axisB_)));

    for (int i = 0; i < 2; ++i) {
        // Correct Jacobian: d/dt[dot(axisB, perp_i)] = (perp_i x axisB)·(wA-wB)
        // With {perp[0], perp[1], n} right-handed: J_0 = perp[0]×n = -perp[1], J_1 = perp[1]×n = +perp[0]
        const mu::Vec3 axisJ = (i == 0) ? -cache_.perpAxes[1] : cache_.perpAxes[0];
        const float contribA = (bodyA_->invMass() > 0.f)
            ? angEff1D(axisJ, bodyA_->invInertiaWorld()) : 0.f;
        const float contribB = (bodyB_->invMass() > 0.f)
            ? angEff1D(axisJ, bodyB_->invInertiaWorld()) : 0.f;
        cache_.angEffMass[i] = 1.f / (contribA + contribB + angularCFM);

        // Bias uses position-level violation: dot(axisB, perp_i) → 0
        const float angViol = mu::dot(axisBWorld, cache_.perpAxes[i]);
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
        cache_.limitEffMass = 1.f / (contribA + contribB + angularCFM);

        if (currentAngle < minAngle_) {
            cache_.limitActive = true;
            cache_.limitLo     = true;
            cache_.limitBias   = (currentAngle - minAngle_) * (kJointBeta * invDt);
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
    for (int i = 0; i < 2; ++i) {
        const mu::Vec3 axisJ = (i == 0) ? -cache_.perpAxes[1] : cache_.perpAxes[0];
        // Scale by 0.8 to dampen energy injection when perpAxes rotates between frames.
        const float warmImp = cache_.angAccImp[i] * 0.8f;
        cache_.angAccImp[i] = warmImp;
        applyAngularImpulsePair(bodyA_, bodyB_, warmImp, axisJ);
    }
    if (cache_.limitActive)
        applyAngularImpulsePair(bodyA_, bodyB_, cache_.limitAccImp, cache_.hingeAxisWorld);
}

void HingeJoint::solveVelocity()
{
    // --- Translational ---
    {
        // damping
        const mu::Vec3 rv = relVelAt(bodyA_, bodyB_, cache_.rA, cache_.rB);
        mu::Vec3 dampingImp = -(rv * cache_.linEffMassInv) * linearDamping_;
        applyLinearImpulsePair(bodyA_, bodyB_, dampingImp, cache_.rA, cache_.rB);

        // impulse
        const mu::Vec3 cVel    = rv + cache_.linBias;
        const mu::Vec3 dLambda = -(cVel * cache_.linEffMassInv);
        cache_.linAccImp = cache_.linAccImp + dLambda;
        applyLinearImpulsePair(bodyA_, bodyB_, dLambda, cache_.rA, cache_.rB);
    }

    // --- Angular Damping (Hinge Axis 기준) ---
    mu::Vec3 relOmega = bodyA_->omega() - bodyB_->omega();
    float rvAng = mu::dot(relOmega, cache_.hingeAxisWorld);
    
    // 자유 회전축에 대한 감쇠 (limitEffMass를 재활용하거나 새로 계산)
    float angDampingImp = -rvAng * cache_.limitEffMass * angularDamping_;
    applyAngularImpulsePair(bodyA_, bodyB_, angDampingImp, cache_.hingeAxisWorld);

    // --- Angular alignment ---
    for (int i = 0; i < 2; ++i) {
        const mu::Vec3 axisJ = (i == 0) ? -cache_.perpAxes[1] : cache_.perpAxes[0];
        JacobianRow row;
        row.bodyA   = bodyA_;
        row.bodyB   = bodyB_;
        row.angA    = axisJ;
        row.angB    = -axisJ;
        row.effMass = cache_.angEffMass[i];
        row.rhs     = cache_.angBias[i];
        solveJacobianRow(row, cache_.angAccImp[i], kAngSOR);
    }

    // --- Limit ---
    if (cache_.limitActive) {
        JacobianRow row;
        row.bodyA   = bodyA_;
        row.bodyB   = bodyB_;
        row.angA    = cache_.hingeAxisWorld;
        row.angB    = -cache_.hingeAxisWorld;
        row.effMass = cache_.limitEffMass;
        row.rhs     = cache_.limitBias;
        row.lower   = cache_.limitLo ? -std::numeric_limits<float>::infinity() : 0.f;
        row.upper   = cache_.limitLo ? 0.f : std::numeric_limits<float>::infinity();
        solveJacobianRow(row, cache_.limitAccImp, kAngSOR);
    }
}

void HingeJoint::solvePosition()
{
    const mu::Vec3 rv      = pseudoRelVelAt(bodyA_, bodyB_, cache_.rA, cache_.rB);
    const mu::Vec3 cVel    = rv + cache_.linPseudoBias;
    const mu::Vec3 dLambda = -(cVel * cache_.linEffMassInv);

    cache_.linPseudoAccImp = cache_.linPseudoAccImp + dLambda;
    applyPseudoLinearImpulsePair(bodyA_, bodyB_, dLambda, cache_.rA, cache_.rB);
}

// ---------------------------------------------------------------------------
// ConeTwistJoint
// ---------------------------------------------------------------------------

ConeTwistJoint::ConeTwistJoint(RigidBody* a, RigidBody* b,
                               mu::Vec3 anchorA, mu::Vec3 anchorB,
                               mu::NQuat refOrientA, mu::NQuat refOrientB,
                               float coneHalfAngle, float twistLimit)
    : ConeTwistJoint(a, b, anchorA, anchorB, refOrientA, refOrientB,
                     mu::Vec3{ 0.f, 0.f, 1.f }, coneHalfAngle, twistLimit)
{}

ConeTwistJoint::ConeTwistJoint(RigidBody* a, RigidBody* b,
                               mu::Vec3 anchorA, mu::Vec3 anchorB,
                               mu::NQuat refOrientA, mu::NQuat refOrientB,
                               mu::Vec3 twistAxisLocalA,
                               float coneHalfAngle, float twistLimit)
    : bodyA_(a), bodyB_(b)
    , anchorA_(anchorA), anchorB_(anchorB)
    , refOrientA_(refOrientA), refOrientB_(refOrientB)
    , twistAxisLocalA_((twistAxisLocalA.len2() > 1e-8f)
        ? mu::Vec3(mu::normalize(twistAxisLocalA))
        : mu::Vec3{ 0.f, 0.f, 1.f })
    , coneHalfAngle_(std::min(coneHalfAngle, mu::pi * 0.85f))
    , twistLimit_(std::abs(twistLimit))
    , linearDamping_(0.1f)
    , coneDamping_(0.1f)
    , twistDamping_(0.1f)
{}

void ConeTwistJoint::resetAnchors()
{
    const mu::Vec3 pivotWorld = bodyB_->pos() + bodyB_->orient().rotate(anchorB_);
    anchorA_ = (~bodyA_->orient()).rotate(pivotWorld - bodyA_->pos());
}

void ConeTwistJoint::prepare(Seconds dt)
{
    const float dtf   = dt.count();
    const float invDt = (dtf > 0.f) ? (1.f / dtf) : 0.f;

    // --- Translational ---
    cache_.rA = bodyA_->orient().rotate(anchorA_);
    cache_.rB = bodyB_->orient().rotate(anchorB_);

    const mu::Vec3 posError = (bodyA_->pos() + cache_.rA)
                            - (bodyB_->pos() + cache_.rB);
    cache_.linearError = posError.len();
    cache_.linEffMassInv = build3x3EffMassInv(bodyA_, cache_.rA, bodyB_, cache_.rB);
    cache_.linBias       = posError * (kLinBeta * invDt);
    cache_.linPseudoBias = posError * (kSplitBeta * invDt);
    cache_.linPseudoAccImp = {};

    // --- Joint-space relative orientation ---
    // q_jointA = refOrientA^-1 * bodyA.orient  (current bodyA orientation in joint space)
    const mu::NQuat qJointA = mulQ(~refOrientA_, bodyA_->orient());
    const mu::NQuat qJointB = mulQ(~refOrientB_, bodyB_->orient());
    // q_rel = qJointA^-1 * qJointB  (relative rotation in joint space)
    const mu::NQuat qRel    = mulQ(~qJointA, qJointB);

    // Twist axis is stored in bodyA/rest joint space. Ragdoll joints pass the
    // parent-to-child bone direction here instead of assuming every bone twists
    // around local Z.
    const mu::Vec3 twistAxisJoint = twistAxisLocalA_;
    const mu::Vec3 twistAxisWorld = mu::Vec3(mu::normalize(bodyA_->orient().rotate(twistAxisJoint)));

    const bool oldConeWarmValid  = cache_.coneWarmValid;
    const bool oldTwistWarmValid = cache_.twistWarmValid;
    const mu::Vec3 oldConeAxis   = cache_.coneAxis;
    const mu::Vec3 oldTwistAxis  = cache_.twistAxis;

    // Decompose relative rotation into swing + twist around the joint Z axis.
    mu::NQuat swing, twist;
    swingTwistDecompose(qRel, twistAxisJoint, swing, twist);

    // --- Cone violation ---
    cache_.coneActive = false;
    cache_.coneViolation = 0.f;
    {
        const float sw = std::clamp(swing.w(), -1.f, 1.f);
        const float swingAngle = 2.f * std::acos(std::abs(sw));

        if (swingAngle > coneHalfAngle_) {
            cache_.coneActive = true;
            const float viol  = swingAngle - coneHalfAngle_;
            cache_.coneViolation = viol;

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
            cache_.coneEffMass      = 1.f / (cA + cB + angularCFM);
            cache_.coneBias         = (std::min(viol, kMaxVelCorrectionAngle) * (kAngBeta   * invDt));
            cache_.conePseudoBias   = (std::min(viol, kMaxSplitCorrectionAngle) * (kSplitBeta * invDt));
            cache_.conePseudoAccImp = 0.f;
            cache_.coneWarmValid    = true;
        } else {
            cache_.coneAccImp       = 0.f;
            cache_.conePseudoAccImp = 0.f;
            cache_.coneWarmValid    = false;
        }
    }

    // --- Twist violation ---
    cache_.twistActive = false;
    cache_.twistViolation = 0.f;
    {
        const float tw = std::clamp(twist.w(), -1.f, 1.f);
        const float twistAngle = 2.f * std::acos(std::abs(tw));
        const mu::Vec3 twistXYZ(twist.x(), twist.y(), twist.z());
        const float sign = (mu::dot(twistXYZ, twistAxisJoint) >= 0.f) ? 1.f : -1.f;
        const float signedTwist = sign * twistAngle;

        if (signedTwist < -twistLimit_ || signedTwist > twistLimit_) {
            cache_.twistActive = true;
            cache_.twistLo     = (signedTwist < -twistLimit_);

            cache_.twistAxis = twistAxisWorld;

            const float cA = (bodyA_->invMass() > 0.f)
                ? angEff1D(twistAxisWorld, bodyA_->invInertiaWorld()) : 0.f;
            const float cB = (bodyB_->invMass() > 0.f)
                ? angEff1D(twistAxisWorld, bodyB_->invInertiaWorld()) : 0.f;
            cache_.twistEffMass      = 1.f / (cA + cB + angularCFM);
            const float limitAngle   = cache_.twistLo ? -twistLimit_ : twistLimit_;
            const float twistError   = signedTwist - limitAngle;
            cache_.twistViolation    = std::abs(twistError);
            cache_.twistBias         = clampAbs(twistError, kMaxVelCorrectionAngle) * (kAngBeta   * invDt);
            cache_.twistPseudoBias   = clampAbs(twistError, kMaxSplitCorrectionAngle) * (kSplitBeta * invDt);
            cache_.twistPseudoAccImp = 0.f;
            cache_.twistWarmValid    = true;
        } else {
            cache_.twistAccImp       = 0.f;
            cache_.twistPseudoAccImp = 0.f;
            cache_.twistWarmValid    = false;
        }
    }
    // Warm-start: linear + angular rows.
    // Cone/twist axes can rotate sharply under large violations. Reusing an
    // impulse along a very different axis injects energy into joint chains, so
    // discard angular warm-start unless the new row is close to last frame's row.
    applyLinearImpulsePair(bodyA_, bodyB_, cache_.linAccImp, cache_.rA, cache_.rB);
    if (cache_.coneActive) {
        const float axisDot = oldConeWarmValid ? std::abs(mu::dot(oldConeAxis, cache_.coneAxis)) : 0.f;
        const float seed = (axisDot >= kWarmStartAxisDotMin)
            ? cache_.coneAccImp * (kWarmStartScale * axisDot)
            : 0.f;
        cache_.coneAccImp = seed;
        if (seed > 0.f)
            applyAngularImpulsePair(bodyA_, bodyB_, seed, cache_.coneAxis);
    }
    if (cache_.twistActive) {
        const float axisDot = oldTwistWarmValid ? std::abs(mu::dot(oldTwistAxis, cache_.twistAxis)) : 0.f;
        const float seed = (axisDot >= kWarmStartAxisDotMin)
            ? cache_.twistAccImp * (kWarmStartScale * axisDot)
            : 0.f;
        cache_.twistAccImp = seed;
        if (seed != 0.f)
            applyAngularImpulsePair(bodyA_, bodyB_, seed, cache_.twistAxis);
    }
}

void ConeTwistJoint::solvePosition()
{
    {
        const mu::Vec3 rv      = pseudoRelVelAt(bodyA_, bodyB_, cache_.rA, cache_.rB);
        const mu::Vec3 cVel    = rv + cache_.linPseudoBias;
        const mu::Vec3 dLambda = -(cVel * cache_.linEffMassInv);
        cache_.linPseudoAccImp = cache_.linPseudoAccImp + dLambda;
        applyPseudoLinearImpulsePair(bodyA_, bodyB_, dLambda, cache_.rA, cache_.rB);
    }

    // Cone angular position correction via split impulse (pseudo-omega).
    // This runs in addition to the velocity-level Baumgarte correction in
    // solveVelocity() and directly adjusts body orientations without injecting
    // energy into the real velocity state.
    if (cache_.coneActive) {
        JacobianRow row;
        row.bodyA   = bodyA_;
        row.bodyB   = bodyB_;
        row.angA    = cache_.coneAxis;
        row.angB    = -cache_.coneAxis;
        row.effMass = cache_.coneEffMass;
        row.rhs     = cache_.conePseudoBias;
        row.lower   = 0.f;
        row.pseudo  = true;
        solveJacobianRow(row, cache_.conePseudoAccImp);
    }

    // Twist angular position correction.
    if (cache_.twistActive) {
        JacobianRow row;
        row.bodyA   = bodyA_;
        row.bodyB   = bodyB_;
        row.angA    = cache_.twistAxis;
        row.angB    = -cache_.twistAxis;
        row.effMass = cache_.twistEffMass;
        row.rhs     = cache_.twistPseudoBias;
        row.lower   = cache_.twistLo ? -std::numeric_limits<float>::infinity() : 0.f;
        row.upper   = cache_.twistLo ? 0.f : std::numeric_limits<float>::infinity();
        row.pseudo  = true;
        solveJacobianRow(row, cache_.twistPseudoAccImp);
    }
}

void ConeTwistJoint::solveVelocity()
{
    // --- Translational ---
    {
        // damping
        const mu::Vec3 rv = relVelAt(bodyA_, bodyB_, cache_.rA, cache_.rB);
        mu::Vec3 dampingImp = -(rv * cache_.linEffMassInv) * linearDamping_;
        applyLinearImpulsePair(bodyA_, bodyB_, dampingImp, cache_.rA, cache_.rB);

        const mu::Vec3 cVel    = rv + cache_.linBias;
        const mu::Vec3 dLambda = -(cVel * cache_.linEffMassInv);
        cache_.linAccImp = cache_.linAccImp + dLambda;
        applyLinearImpulsePair(bodyA_, bodyB_, dLambda, cache_.rA, cache_.rB);
    }

    // --- 3D Angular Damping ---
    mu::Vec3 relOmega = bodyA_->omega() - bodyB_->omega();
    
    // 각 축별로 Effective Mass가 다르지만, 안정성을 위해 
    // ConeAxis와 TwistAxis의 평균적인 EffMass를 사용하거나 
    // 각 축에 투영해서 개별 적용
    
    // 1. Twist 축 감쇄
    float rvTwist = mu::dot(relOmega, cache_.twistAxis);
    float twistDamp = -rvTwist * cache_.twistEffMass * twistDamping_;
    applyAngularImpulsePair(bodyA_, bodyB_, twistDamp, cache_.twistAxis);

    // 2. Cone(Swing) 축 감쇄
    if (cache_.coneActive) {
        float rvCone = mu::dot(relOmega, cache_.coneAxis);
        float coneDamp = -rvCone * cache_.coneEffMass * coneDamping_;
        applyAngularImpulsePair(bodyA_, bodyB_, coneDamp, cache_.coneAxis);
    } else {
        // 제약 조건이 활성화되지 않았더라도 묵직함을 위해 
        // 임의의 직교축(Perp)에 대해 감쇄를 줄 수 있음
    }

    // --- Cone ---
    if (cache_.coneActive) {
        JacobianRow row;
        row.bodyA   = bodyA_;
        row.bodyB   = bodyB_;
        row.angA    = cache_.coneAxis;
        row.angB    = -cache_.coneAxis;
        row.effMass = cache_.coneEffMass;
        row.rhs     = cache_.coneBias;
        row.lower   = 0.f;
        solveJacobianRow(row, cache_.coneAccImp, kAngSOR);
    }

    // --- Twist ---
    if (cache_.twistActive) {
        JacobianRow row;
        row.bodyA   = bodyA_;
        row.bodyB   = bodyB_;
        row.angA    = cache_.twistAxis;
        row.angB    = -cache_.twistAxis;
        row.effMass = cache_.twistEffMass;
        row.rhs     = cache_.twistBias;
        row.lower   = cache_.twistLo ? -std::numeric_limits<float>::infinity() : 0.f;
        row.upper   = cache_.twistLo ? 0.f : std::numeric_limits<float>::infinity();
        solveJacobianRow(row, cache_.twistAccImp, kAngSOR);
    }
}