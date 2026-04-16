#ifndef __ActiveRagdoll_HPP
#define __ActiveRagdoll_HPP

#include "ragdoll.hpp"
#include "animation.hpp"

// ---------------------------------------------------------------------------
// ActiveRagdollController
//
// Drives a Ragdoll's Dynamic bodies toward a target animation pose via
// per-joint PD torques.  Applied once per physics tick (before PhysicsWorld::step).
//
// PD torque formula (per bone):
//   q     = mulQ(targetOrient, conj(body->orient()))  -- error quaternion
//   if q.w < 0: q = -q                               -- shortest-path guarantee
//   torque = kp * q.xyz * 2 - kd * body->omega()
//   body->applyTorqueImpulse(torque * dt)
//
// Usage:
//   ActiveRagdollController ctrl;
//   ctrl.setDefaultGains({300.f, 30.f});
//   // each tick before PhysicsWorld::step:
//   ctrl.update(ragdoll, skel, targetPose, dt);
//   // on hit:
//   ctrl.onImpact(impulseStrength);
// ---------------------------------------------------------------------------
class ActiveRagdollController {
public:
    struct Gains {
        float kp = 300.f;   // proportional gain (stiffness)
        float kd = 30.f;    // derivative  gain  (damping)
    };

    // Drive all Dynamic ragdoll bones toward targetPose.
    // targetPose[bone.boneIdx] = parent-relative AnimFrame (same convention as
    // AnimBlender localXformData / syncFromPose).
    // objectWorldMat is the object's current world transform.
    // dt is the physics sub-step duration.
    void update(Ragdoll& ragdoll,
                const Skeleton& skel,
                const std::vector<AnimFrame>& targetPose,
                mu::Mat4x4 objectWorldMat,
                Seconds dt);

    // Called when the character receives a physics impact.
    // Temporarily reduces kp (stiffness) so the body goes limp; kd stays
    // intact to continue absorbing energy.
    // impulseStrength is informational and may be used to scale the duration.
    void onImpact(float impulseStrength);

    void setDefaultGains(Gains g) { defaultGains_ = g; }
    Gains defaultGains() const    { return defaultGains_; }

private:
    Gains defaultGains_{ 300.f, 30.f };

    float impactRecoveryTimer_ = 0.f;   // seconds remaining in limp state
    float impactRecoveryTime_  = 0.5f;  // full recovery duration
    float currentKpScale_      = 1.f;   // [0, 1]; 0 = fully limp, 1 = normal
};

#endif // __ActiveRagdoll_HPP
