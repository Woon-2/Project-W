#ifndef room_server_physicsWorld_hpp
#define room_server_physicsWorld_hpp

#include "rigidBody.hpp"
#include "constraint.hpp"
#include "contactConstraint.hpp"
#include "broadPhase.hpp"
#include "collision.hpp"
#include "terrain.hpp"
#include <functional>
#include <unordered_map>
#include <utility>

// PhysicsWorld manages the physics simulation tick.
// It holds non-owning pointers to RigidBody instances (owned inline by Object).
//
// Pipeline per step():
//   integrate()          - apply forces, integrate velocities + positions
//   generateContacts()   - broad phase -> narrow phase -> ContactConstraints
//   solveConstraints()   - PGS: prepare -> N x solveVelocity -> solvePosition
//
// Usage:
//   1. After creating a game object, call registerBody(&obj->body(), rebuildCb).
//   2. Call step(dt) once per physics tick.
class PhysicsWorld {
public:
    PhysicsWorld();

    // Register a body for simulation.
    // onRebuildBVH is called after integration to keep the BVH in sync.
    void registerBody(RigidBody* body,
                      std::function<void()> onRebuildBVH = {});

    // Remove a body from simulation. Safe to call with an unregistered body.
    void unregisterBody(RigidBody* body);

    // Main simulation tick (integrate + detect + solve).
    void step(Seconds dt);

    // Register a static height-field terrain for body-terrain collision.
    // terrainBody must be MotionType::Static; it is NOT added to the broad phase.
    void registerTerrain(RigidBody* terrainBody, const TerrainHeightField* heightField);

    // Remove the terrain collider. Safe to call when no terrain is registered.
    void unregisterTerrain();

    // Set the gravitational acceleration applied to Dynamic bodies each step.
    void MU_CALLCONV setGravity(mu::Vec3 g) { gravity_ = g; }

    // Set the number of PGS velocity iterations per step (default: 10).
    void setSolverIterations(int n) { solverIterations_ = n; }

    // Set the number of Split Impulse position correction iterations (default: 3).
    void setPositionSolveIterations(int n) { positionSolveIterations_ = n; }

    // Set the number of physics sub-steps per step() call (default: 2).
    void setSubStepCount(int n) { subStepCount_ = n; }

    // Interpolation helpers. t=0 -> prev state, t=1 -> curr state.
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

    std::vector<Entry>                              entries_;
    mu::Vec3                                        gravity_{ 0.f, -9.8f, 0.f };
    std::unique_ptr<BroadPhase>                     broadPhase_;
    std::vector<std::unique_ptr<ContactConstraint>> contactConstraints_;
    std::vector<std::unique_ptr<Constraint>>        jointConstraints_;

    // Optional terrain collider (null when no terrain is registered).
    // Terrain body is NOT in entries_ or broadPhase_; TerrainCollider
    // iterates Dynamic bodies directly.
    std::unique_ptr<TerrainCollider>                terrainCollider_;

    int     solverIterations_         = 10;
    int     positionSolveIterations_  = 3;
    int     subStepCount_             = 2;
    Seconds currentSubDt_{};

    struct WarmEntry {
        float accNormal     = 0.f;
        float accTangent[2] = {};
    };
    struct WarmPairHash {
        size_t operator()(std::pair<RigidBody*, RigidBody*> p) const noexcept {
            auto h1 = std::hash<RigidBody*>{}(p.first);
            auto h2 = std::hash<RigidBody*>{}(p.second);
            return h1 ^ (h2 * 2654435761u);
        }
    };
    static std::pair<RigidBody*, RigidBody*> normKey(RigidBody* a, RigidBody* b) {
        return (a < b) ? std::make_pair(a, b) : std::make_pair(b, a);
    }
    std::unordered_map<std::pair<RigidBody*, RigidBody*>, WarmEntry, WarmPairHash> warmCache_;
};

#endif // room_server_physicsWorld_hpp
