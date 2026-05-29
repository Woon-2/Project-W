#include "pch.hpp"
#include "activeRagdoll.hpp"
#include "physicsWorld.hpp"
#include "mesh.hpp"

// ---------------------------------------------------------------------------
// Shared helpers (mirrors ragdoll.cpp conventions)
// ---------------------------------------------------------------------------

// Multiply two normalised quaternions.
static mu::NQuat mulQ(mu::NQuat a, mu::NQuat b)
{
    return mu::NQuat(mu::Quat(a) * mu::Quat(b));
}

// ---------------------------------------------------------------------------
// computeOrientError  (file-scope helper)
//
// World-frame error rotation that drives current -> target: q = conj(current) * target,
// i.e. R(current)^-1 * R(target) in row-vector convention (= Unity's
// target * Inverse(current)). Its xyz axis lives in WORLD space, matching the
// world-frame torque applied via applyTorqueImpulse(omega += tau * invInertiaWorld).
// Using target * conj(current) would yield a body-frame axis and steer the torque
// about the wrong world axis once the bone is significantly rotated.
// Negate if q.w < 0 (shortest-path). Small-angle approximation: axis*angle ~= q.xyz * 2.
// ---------------------------------------------------------------------------

static mu::Vec3 MU_CALLCONV computeOrientError(mu::NQuat current, mu::NQuat target)
{
    mu::NQuat q = mulQ(~current, target);
    if (q.w() < 0.f)
        q = -q;
    return mu::Vec3(q.x() * 2.f, q.y() * 2.f, q.z() * 2.f);
}

// ---------------------------------------------------------------------------
// ActiveRagdollController::onImpact
// ---------------------------------------------------------------------------

void ActiveRagdollController::onImpact(float /*impulseStrength*/)
{
    impactRecoveryTimer_ = impactRecoveryTime_;
    currentKpScale_      = 0.f;
}

// ---------------------------------------------------------------------------
// ActiveRagdollController::update
//
// Traverses the skeleton DFS to compute each bone's target world orientation
// from the target pose (same composition as syncFromPoseDFS in ragdoll.cpp):
//   boneWorldMat = localAnimMat * parentWorldMat   (row-vector convention)
//
// For each ragdoll bone that is Dynamic, applies a PD torque impulse.
// ---------------------------------------------------------------------------

// Internal recursive DFS state.
struct PDUpdateCtx {
    const Ragdoll&                            ragdoll;
    const std::vector<AnimFrame>&             targetPose;
    const ActiveRagdollController::Gains*     gains;
    Seconds                                   dt;
};

static void pdDFS(const Bone* bone,
                  mu::Mat4x4  parentWorldMat,
                  const PDUpdateCtx& ctx)
{
    // Target world matrix for this bone.
    const mu::Mat4x4 boneWorldMat =
        convertAnimFrameToMatrix(ctx.targetPose[bone->boneIdx]) * parentWorldMat;

    // Apply PD torque if this bone has a Dynamic ragdoll body.
    for (const RagdollBone& rb : ctx.ragdoll.bones()) {
        if (rb.boneIdx != bone->boneIdx) continue;
        if (rb.body->motionType() != MotionType::Dynamic) break;

        // Target orientation comes from the target pose world matrix.
        const mu::NQuat targetOrient =
            mu::NQuat(mu::quatRotMat(boneWorldMat.get()));

        const mu::Vec3 error = computeOrientError(rb.body->orient(), targetOrient);

        const float kp = ctx.gains->kp;
        const float kd = ctx.gains->kd;
        const float dtf = ctx.dt.count();

        const mu::Vec3 torque = error * kp - rb.body->omega() * kd;
        rb.body->applyTorqueImpulse(torque * dtf);
        break;
    }

    for (const Bone* child : bone->children)
        pdDFS(child, boneWorldMat, ctx);
}

void ActiveRagdollController::update(Ragdoll& ragdoll,
                                      const Skeleton& skel,
                                      const std::vector<AnimFrame>& targetPose,
                                      mu::Mat4x4 objectWorldMat,
                                      Seconds dt)
{
    if (!ragdoll.isActive() || !skel.pRoot) return;

    // Tick the impact recovery timer and interpolate kp scale.
    if (impactRecoveryTimer_ > 0.f) {
        impactRecoveryTimer_ -= dt.count();
        if (impactRecoveryTimer_ <= 0.f) {
            impactRecoveryTimer_ = 0.f;
            currentKpScale_      = 1.f;
        } else {
            // Linear recovery from 0 toward 1.
            currentKpScale_ = 1.f - (impactRecoveryTimer_ / impactRecoveryTime_);
        }
    }

    // Build resolved gains with kp scaled by impact state.
    const Gains gains{ defaultGains_.kp * currentKpScale_, defaultGains_.kd };

    const PDUpdateCtx ctx{ ragdoll, targetPose, &gains, dt };
    pdDFS(skel.pRoot, objectWorldMat, ctx);
}
