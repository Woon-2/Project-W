#ifndef __RigidBody_HPP
#define __RigidBody_HPP

#include "collision.hpp"

// Motion type for a RigidBody.
// Kinematic: game logic drives velocity; solver does not apply forces.
// Dynamic:   physics drives via force/impulse accumulation (Phase 2+).
// Static:    never moves; treated as infinite mass by the solver.
enum class MotionType { Kinematic, Dynamic, Static };

// Kinematic state snapshot used for render interpolation.
// prev and curr are kept separate so the renderer can lerp between them.
// Note: BVH is NOT part of BodyState - it lives directly in RigidBody
// so there is no per-snapshot BVH to copy.
struct BodyState {
    mu::Vec3  pos{};
    mu::Vec3  linearVel{};
    mu::Vec3  omega{};
    mu::NQuat orient{};
    mu::Vec3  scale{ 1.f, 1.f, 1.f };
};

// Diagonal 3x3 inertia tensor helper: (Ixx, Iyy, Izz).
// Returns I_local as a full Mat3x3 with zeros off-diagonal.
mu::Mat3x3 computeBoxInertia(float mass, mu::Vec3 halfExtents);
mu::Mat3x3 computeCapsuleInertia(float mass, float radius, float halfHeight);

// Physics representation for a single simulated body.
// Owned inline by Object (for game entities) or by PhysicsWorld
// via unique_ptr (for future ragdoll bones).
//
// Double-buffered state (prev/curr) supports render interpolation at
// sub-physics-tick granularity without extra copies.
class RigidBody {
public:
    explicit RigidBody(MotionType type = MotionType::Kinematic) : type_(type) {}

    void setMotionType(MotionType t) { type_ = t; }
    MotionType motionType() const { return type_; }

    // --- state setters (write to curr only) ---
    void MU_CALLCONV setPos(mu::Vec3 p)       { curr_.pos       = p; }
    void MU_CALLCONV setLinearVel(mu::Vec3 v) { curr_.linearVel = v; }
    void MU_CALLCONV setOmega(mu::Vec3 w)     { curr_.omega     = w; }
    void MU_CALLCONV setOrient(mu::NQuat q)   { curr_.orient    = q; }
    void MU_CALLCONV setScale(mu::Vec3 s)     { curr_.scale     = s; }

    // --- state getters (read from curr) ---
    mu::Vec3  MU_CALLCONV pos()       const { return curr_.pos; }
    mu::Vec3  MU_CALLCONV linearVel() const { return curr_.linearVel; }
    mu::Vec3  MU_CALLCONV omega()     const { return curr_.omega; }
    mu::NQuat MU_CALLCONV orient()    const { return curr_.orient; }
    mu::Vec3  MU_CALLCONV scale()     const { return curr_.scale; }

    // Called at the start of each PhysicsWorld::step() to advance the
    // double buffer: prev <- curr. After this, curr can be freely updated
    // while prev holds the state at the beginning of the tick.
    void advanceState() { prev_ = curr_; }

    // Snap prev to match curr exactly (use after teleportation or initial
    // placement to avoid a one-frame interpolation artifact).
    void snapToCurrent() { prev_ = curr_; }

    // Read-only access to both snapshots for the renderer.
    const BodyState& curr() const { return curr_; }
    const BodyState& prev() const { return prev_; }

    // World-space BVH. Rebuilt by Object::rebuildBodyBVH() whenever
    // position, orientation, scale, or animation pose changes.
    const BVH& worldBVH() const { return worldBVH_; }
    BVH&       worldBVH()       { return worldBVH_; }

    // ---------------------------------------------------------------
    // Phase 2: Dynamic body mass/inertia/force API
    // ---------------------------------------------------------------

    // Set the body's mass. Automatically initialises invInertiaLocal_ to
    // a unit-half-extent box inertia if localInertia is not set explicitly.
    // invMass_ = 0 implies infinite mass (Static or Kinematic use only).
    void setMass(float mass);

    // Override the local-space inertia tensor (must be positive-definite).
    // Stores its inverse internally.
    void setInertia(const mu::Mat3x3& localInertia);

    // Per-step velocity decay coefficients in [0, 1).
    // A value of 0.05 damps ~5% of velocity per second at small dt.
    void setLinearDamping(float d)  { linearDamping_  = d; }
    void setAngularDamping(float d) { angularDamping_ = d; }

    // Lock all rotational DOF: sets invInertia to zero so no impulse or torque
    // can rotate this body. Orientation is controlled solely by setOrient().
    // Call after setMass(). Replaces the setAngularDamping(large_value) hack.
    void lockRotation();

    // Strength of the gravity-aligned self-righting torque (N·m per radian of tilt).
    // Each integrate step a corrective torque = cross(bodyUp, worldUp) * stiffness
    // is applied, pulling the body's local Y-axis toward world Y-up.
    // 0 (default) disables the correction; ~500 gives a 1–2 s correction time.
    void  setUprightStiffness(float k) { uprightStiffness_ = k; }
    float uprightStiffness()     const { return uprightStiffness_; }

    // Per-body gravity multiplier (engine-standard, mirrors Unity's
    // Rigidbody.gravityScale). 1 = full world gravity, 0 = no gravity for this
    // body. The integrate step multiplies the world gravity by this value, so a
    // character controller can gate gravity off while grounded (eliminating the
    // gravity-vs-contact micro-bounce) and restore it instantly when airborne.
    // Only meaningful for Dynamic bodies.
    void  setGravityScale(float s) { gravityScale_ = s; }
    float gravityScale()     const { return gravityScale_; }

    // Coefficient of restitution (0 = perfectly inelastic, 1 = elastic).
    void setRestitution(float r)    { restitution_ = r; }

    // Coulomb friction coefficient. Used by ContactConstraint for tangent impulse clamping.
    void setFriction(float f)       { friction_ = f; }

    // Accumulate a world-space force (applied at centre of mass).
    void MU_CALLCONV applyForce(mu::Vec3 f);

    // Accumulate a world-space force applied at a world-space point.
    // Generates both a linear force and a torque about the centre of mass.
    void MU_CALLCONV applyForceAtPoint(mu::Vec3 f, mu::Vec3 worldPoint);

    // Apply an instantaneous world-space linear impulse at a world-space point.
    // Directly modifies linearVel_ and omega_ (bypasses the force accumulator).
    void MU_CALLCONV applyImpulse(mu::Vec3 j, mu::Vec3 worldPoint);

    // Apply an instantaneous angular impulse (torque impulse) in world space.
    // Directly modifies omega_ without affecting linear velocity.
    // Used by joint constraints and ActiveRagdollController.
    void MU_CALLCONV applyTorqueImpulse(mu::Vec3 tau);

    // Zero the force and torque accumulators.  Call at the end of each tick.
    void clearAccumulators();

    // Pseudo-velocity API for Split Impulse position correction.
    // pseudo-velocities are accumulated during solvePosition() and applied
    // once to position at the end of each step; they never affect real velocity.
    mu::Vec3 pseudoLinearVel() const { return pseudoLinearVel_; }
    mu::Vec3 pseudoOmega()     const { return pseudoOmega_; }
    void MU_CALLCONV addPseudoLinearVel(mu::Vec3 v) { pseudoLinearVel_ += v; }
    void MU_CALLCONV addPseudoOmega    (mu::Vec3 w) { pseudoOmega_     += w; }
    void clearPseudoVelocities() { pseudoLinearVel_ = {}; pseudoOmega_ = {}; }
    void applyPseudoVelocity(Seconds dt) {
        if (type_ == MotionType::Static || type_ == MotionType::Kinematic) return;
        const float dtf = dt.count();
        curr_.pos += pseudoLinearVel_ * dtf;
        // Angular position correction: integrate pseudo-omega into orientation.
        // Used by ConeTwistJoint::solvePosition() to correct angular limit violations
        // without affecting the real velocity state (split impulse, energy-neutral).
        if (pseudoOmega_.len2() > 1e-10f) {
            const auto wq = mu::Quat(pseudoOmega_, 0.f);
            auto newOrient = mu::Quat(curr_.orient) + mu::Quat(curr_.orient) * wq * 0.5f * dtf;
            curr_.orient = mu::NQuat{ newOrient };
        }
    }

    // Getters used by PhysicsWorld::integrate().
    // Non-Dynamic bodies always appear as infinite mass (0) to the constraint solver,
    // even when setMass() was called.  This prevents the solver from applying
    // impulses to Kinematic or Static bodies whose positions are externally driven.
    float            invMass()          const { return (type_ == MotionType::Dynamic) ? invMass_ : 0.f; }
    float            restitution()      const { return restitution_; }
    float            friction()         const { return friction_; }
    float            linearDamping()    const { return linearDamping_; }
    float            angularDamping()   const { return angularDamping_; }
    mu::Vec3         forceAccum()       const { return forceAccum_; }
    mu::Vec3         torqueAccum()      const { return torqueAccum_; }
    const mu::Mat3x3& invInertiaWorld() const { return invInertiaWorld_; }

    // World-space AABB used by the broad phase.
    // Returns the root BVH node bounds if the BVH is non-empty;
    // otherwise a unit cube centred at the current position.
    AABB worldAABB() const {
        if (!worldBVH_.empty())
            return worldBVH_.nodes[0].bounds;
        return AABB{ curr_.pos, mu::Vec3(1.f, 1.f, 1.f) };
    }

    // Recompute invInertiaWorld_ from the current orientation and invInertiaLocal_.
    // Called by PhysicsWorld::integrate() after updating orientation.
    // No-op when rotation is locked (invInertiaWorld_ stays zero).
    void updateInertiaWorld();

    // Arbitrary pointer for associating a game-layer owner (e.g. Object*) with this body.
    // PhysicsWorld does not interpret it; the game sets and reads it freely.
    void  setUserData(void* data) { userData_ = data; }
    void* userData()        const { return userData_; }

private:
    MotionType type_;
    BodyState  curr_{};
    BodyState  prev_{};
    BVH        worldBVH_{};
    void*      userData_ = nullptr;

    // --- Dynamic body properties (unused for Kinematic/Static) ---
    bool       rotationLocked_   = false;
    float      uprightStiffness_ = 0.f;
    float      gravityScale_     = 1.f;   // per-body gravity multiplier (see setGravityScale)
    float      invMass_          = 0.f;
    float      restitution_    = 0.3f;
    float      friction_       = 0.5f;
    float      linearDamping_  = 0.05f;
    float      angularDamping_ = 0.05f;
    mu::Mat3x3 invInertiaLocal_{};   // local-space inverse inertia tensor
    mu::Mat3x3 invInertiaWorld_{};   // world-space inverse inertia tensor
    mu::Vec3   forceAccum_{};        // accumulated force  (reset each step)
    mu::Vec3   torqueAccum_{};       // accumulated torque (reset each step)
    mu::Vec3   pseudoLinearVel_{};   // split-impulse position correction velocity (reset each step)
    mu::Vec3   pseudoOmega_{};       // split-impulse angular correction          (reset each step)
};

#endif // __RigidBody_HPP
