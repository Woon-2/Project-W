#ifndef room_server_rigidBody_hpp
#define room_server_rigidBody_hpp

#include "collision.hpp"

// Motion type for a RigidBody.
// Kinematic: game logic drives velocity; solver does not apply forces.
// Dynamic:   physics drives via force/impulse accumulation.
// Static:    never moves; treated as infinite mass by the solver.
enum class MotionType { Kinematic, Dynamic, Static };

// Kinematic state snapshot used for interpolation.
// prev and curr are kept separate so two snapshots can be compared.
// Note: BVH is NOT part of BodyState - it lives directly in RigidBody.
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

// AI velocity motor: drives actual velocity toward desiredVel via
// per-frame corrective impulses (XZ only; Y is under gravity control).
// Applied inside PhysicsWorld::integrate() for Dynamic bodies.
struct VelocityMotor {
    mu::Vec3 desiredVel{};
    float    maxAcceleration{ 20.f };  // m/s² — toward desired
    float    maxDeceleration{ 40.f };  // m/s² — braking
    float    gain           { 10.f };  // proportional gain (≈ 1/convergence_time_s)
    bool     enabled        { false };
};

// Physics representation for a single simulated body.
// Owned inline by Object (for game entities).
//
// Double-buffered state (prev/curr) supports interpolation at
// sub-physics-tick granularity without extra copies.
class RigidBody {
public:
    explicit RigidBody(MotionType type = MotionType::Kinematic) : type_(type) {}

    void setMotionType(MotionType t) { type_ = t; }
    MotionType motionType() const { return type_; }

    // --- state setters (write to curr only) ---
    void setPos(mu::Vec3 p)       { curr_.pos       = p; }
    void setLinearVel(mu::Vec3 v) { curr_.linearVel = v; }
    void setOmega(mu::Vec3 w)     { curr_.omega     = w; }
    void setOrient(mu::NQuat q)   { curr_.orient    = q; }
    void setScale(mu::Vec3 s)     { curr_.scale     = s; }

    // --- state getters (read from curr) ---
    mu::Vec3  pos()       const { return curr_.pos; }
    mu::Vec3  linearVel() const { return curr_.linearVel; }
    mu::Vec3  omega()     const { return curr_.omega; }
    mu::NQuat orient()    const { return curr_.orient; }
    mu::Vec3  scale()     const { return curr_.scale; }

    // Called at the start of each PhysicsWorld::step() to advance the
    // double buffer: prev <- curr. After this, curr can be freely updated
    // while prev holds the state at the beginning of the tick.
    void advanceState() { prev_ = curr_; }

    // Snap prev to match curr exactly (use after teleportation or initial
    // placement to avoid a one-frame interpolation artifact).
    void snapToCurrent() { prev_ = curr_; }

    // Read-only access to both snapshots.
    const BodyState& curr() const { return curr_; }
    const BodyState& prev() const { return prev_; }

    // World-space BVH. Rebuilt by Object::rebuildBodyBVH() whenever
    // position, orientation, scale, or animation pose changes.
    const BVH& worldBVH() const { return worldBVH_; }
    BVH&       worldBVH()       { return worldBVH_; }

    // ---------------------------------------------------------------
    // Dynamic body mass/inertia/force API
    // ---------------------------------------------------------------

    // Set the body's mass. Automatically initialises invInertiaLocal_ to
    // a unit-half-extent box inertia if localInertia is not set explicitly.
    // invMass_ = 0 implies infinite mass (Static or Kinematic use only).
    void setMass(float mass);

    // Override the local-space inertia tensor (must be positive-definite).
    // Stores its inverse internally.
    void setInertia(const mu::Mat3x3& localInertia);

    // Per-step velocity decay coefficients in [0, 1).
    void setLinearDamping(float d)  { linearDamping_  = d; }
    void setAngularDamping(float d) { angularDamping_ = d; }

    // Lock all rotational DOF: sets invInertia to zero so no impulse or torque
    // can rotate this body. Orientation is controlled solely by setOrient().
    // Call after setMass().
    void lockRotation();

    // Gravity-aligned self-righting torque strength (N·m per radian of tilt).
    // 0 = disabled. ~500 gives ~1 s correction from a 10° tilt.
    void  setUprightStiffness(float k) { uprightStiffness_ = k; }
    float uprightStiffness()     const { return uprightStiffness_; }

    // Coefficient of restitution (0 = perfectly inelastic, 1 = elastic).
    void setRestitution(float r)    { restitution_ = r; }

    // Coulomb friction coefficient.
    void setFriction(float f)       { friction_ = f; }

    // Accumulate a world-space force (applied at centre of mass).
    void MU_CALLCONV applyForce(mu::Vec3 f);

    // Accumulate a world-space force applied at a world-space point.
    void MU_CALLCONV applyForceAtPoint(mu::Vec3 f, mu::Vec3 worldPoint);

    // Apply an instantaneous world-space linear impulse at a world-space point.
    void MU_CALLCONV applyImpulse(mu::Vec3 j, mu::Vec3 worldPoint);

    // Zero the force and torque accumulators.
    void clearAccumulators();

    // Velocity motor API (see VelocityMotor above).
    void     enableMotor(bool v)                    { motor_.enabled = v; }
    bool     motorEnabled()                   const { return motor_.enabled; }
    void MU_CALLCONV setDesiredVel(mu::Vec3 v)      { motor_.desiredVel = v; }
    mu::Vec3 desiredVel()                     const { return motor_.desiredVel; }
    void     setMotorMaxAcceleration(float a)       { motor_.maxAcceleration = a; }
    void     setMotorMaxDeceleration(float d)       { motor_.maxDeceleration = d; }
    void     setMotorGain(float g)                  { motor_.gain = g; }
    const VelocityMotor& motor()              const { return motor_; }

    // Pseudo-velocity API for Split Impulse position correction.
    mu::Vec3 pseudoLinearVel() const { return pseudoLinearVel_; }
    mu::Vec3 pseudoOmega()     const { return pseudoOmega_; }
    void addPseudoLinearVel(mu::Vec3 v) { pseudoLinearVel_ += v; }
    void addPseudoOmega    (mu::Vec3 w) { pseudoOmega_     += w; }
    void clearPseudoVelocities() { pseudoLinearVel_ = {}; pseudoOmega_ = {}; }
    void applyPseudoVelocity(Seconds dt) {
        if (type_ == MotionType::Static || type_ == MotionType::Kinematic) return;
        curr_.pos += pseudoLinearVel_ * dt.count();
    }

    // Getters used by PhysicsWorld::integrate().
    // Non-Dynamic bodies always appear as infinite mass (0) to the constraint solver.
    float            invMass()          const { return (type_ == MotionType::Dynamic) ? invMass_ : 0.f; }
    float            restitution()      const { return restitution_; }
    float            friction()         const { return friction_; }
    float            linearDamping()    const { return linearDamping_; }
    float            angularDamping()   const { return angularDamping_; }
    mu::Vec3         forceAccum()       const { return forceAccum_; }
    mu::Vec3         torqueAccum()      const { return torqueAccum_; }
    const mu::Mat3x3& invInertiaWorld() const { return invInertiaWorld_; }

    // World-space AABB used by the broad phase.
    AABB worldAABB() const {
        if (!worldBVH_.empty())
            return worldBVH_.nodes[0].bounds;
        return AABB{ curr_.pos, mu::Vec3(1.f, 1.f, 1.f) };
    }

    // Recompute invInertiaWorld_ from the current orientation and invInertiaLocal_.
    // No-op when rotation is locked (invInertiaWorld_ stays zero).
    void updateInertiaWorld();

private:
    MotionType type_;
    BodyState  curr_{};
    BodyState  prev_{};
    BVH        worldBVH_{};

    // --- Dynamic body properties (unused for Kinematic/Static) ---
    bool       rotationLocked_   = false;
    float      uprightStiffness_ = 0.f;
    float      invMass_          = 0.f;
    float      restitution_    = 0.3f;
    float      friction_       = 0.5f;
    float      linearDamping_  = 0.05f;
    float      angularDamping_ = 0.05f;
    mu::Mat3x3 invInertiaLocal_{};
    mu::Mat3x3 invInertiaWorld_{};
    mu::Vec3   forceAccum_{};
    mu::Vec3   torqueAccum_{};
    mu::Vec3   pseudoLinearVel_{};
    mu::Vec3   pseudoOmega_{};

    VelocityMotor motor_{};
};

#endif // room_server_rigidBody_hpp
