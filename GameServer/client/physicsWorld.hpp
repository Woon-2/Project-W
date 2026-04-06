#ifndef __PhysicsWorld_HPP
#define __PhysicsWorld_HPP

#include "rigidBody.hpp"
#include "constraint.hpp"
#include "contactConstraint.hpp"
#include "broadPhase.hpp"
#include "collision.hpp"
#include <functional>

struct TerrainHeightField;

// PhysicsWorld manages the physics simulation tick.
// It holds non-owning pointers to RigidBody instances (which are owned
// inline by Object for game entities, or via unique_ptr for ragdoll bones
// added in a later phase).
//
// Pipeline per step():
//   integrate()          - apply forces, integrate velocities + positions
//   generateContacts()   - broad phase -> narrow phase -> ContactConstraints
//   solveConstraints()   - PGS: prepare -> N x solveVelocity -> solvePosition
//
// Usage:
//   1. After creating a game object, call registerBody(&obj->body(), rebuildCb).
//   2. Call step(dt) once per physics tick.
//   3. Use interpolatePos/interpolateOrient in Object::update() for rendering.
class PhysicsWorld {
public:
    PhysicsWorld();

    // Register a body for simulation.
    // onRebuildBVH is called after integration to keep the BVH in sync.
    // Pass an empty function if the body has no BVH (e.g. a simple trigger).
    void registerBody(RigidBody* body,
                      std::function<void()> onRebuildBVH = {});

    // Remove a body from simulation. Safe to call with an unregistered body.
    void unregisterBody(RigidBody* body);

    // Register a static height-field terrain for body-terrain collision.
    // terrainBody must be MotionType::Static; it is NOT added to the broad phase.
    void registerTerrain(RigidBody* terrainBody, const TerrainHeightField* heightField);

    // Remove the terrain collider. Safe to call when no terrain is registered.
    void unregisterTerrain();

    // Main simulation tick (integrate + detect + solve).
    void step(Seconds dt);

    // Set the gravitational acceleration applied to Dynamic bodies each step.
    // Default: {0, -9.8, 0} (world Y-up).
    void MU_CALLCONV setGravity(mu::Vec3 g) { gravity_ = g; }

    // Set the number of PGS velocity iterations per step (default: 10).
    void setSolverIterations(int n) { solverIterations_ = n; }

    // Render-interpolation helpers. t=0 -> prev state, t=1 -> curr state.
    static mu::Vec3  MU_CALLCONV interpolatePos(const RigidBody& b, float t);
    static mu::NQuat MU_CALLCONV interpolateOrient(const RigidBody& b, float t);

private:
    void integrate(Seconds dt);
    void generateContacts();
    void solveConstraints(Seconds dt);

    struct Entry {
        RigidBody*            body;
        std::function<void()> onRebuildBVH;
    };

    std::vector<Entry>                          entries_;
    mu::Vec3                                    gravity_{ 0.f, -9.8f, 0.f };

    // Broad-phase collision detector (swappable; default BruteForceBroadPhase).
    std::unique_ptr<BroadPhase>                 broadPhase_;

    // Per-step contact constraints (rebuilt every step).
    std::vector<std::unique_ptr<ContactConstraint>> contactConstraints_;

    // Persistent constraints: joints, springs, etc. (added by the game).
    std::vector<std::unique_ptr<Constraint>>    jointConstraints_;

    // Optional terrain collider (null when no terrain is registered).
    // Terrain body is NOT in entries_ or broadPhase_; TerrainCollider
    // iterates Dynamic bodies directly.
    std::unique_ptr<TerrainCollider>            terrainCollider_;

    int solverIterations_ = 10;
};

#endif // __PhysicsWorld_HPP
