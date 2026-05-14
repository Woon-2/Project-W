#ifndef __PhysicsWorld_HPP
#define __PhysicsWorld_HPP

#include "rigidBody.hpp"
#include "constraint.hpp"
#include "contactConstraint.hpp"
#include "broadPhase.hpp"
#include "collision.hpp"
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

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
    // collisionGroup / collisionMask: bit-field pair for filtering.
    // A pair (a, b) is skipped if (a.collisionGroup & b.collisionMask) == 0.
    // Default (group=1, mask=0xFFFF) matches all bodies.
    void registerBody(RigidBody* body,
                      std::function<void()> onRebuildBVH = {},
                      uint16_t collisionGroup = 1,
                      uint16_t collisionMask  = 0xFFFF);

    // Remove a body from simulation. Safe to call with an unregistered body.
    void unregisterBody(RigidBody* body);

    // --- Joint constraint API ---
    // Owning: PhysicsWorld takes ownership. Use for standalone joints.
    void addJointConstraint(std::unique_ptr<Constraint> joint);
    void removeJointConstraint(Constraint* joint);

    // Non-owning: caller retains ownership (e.g. Ragdoll). Use for ragdoll joints.
    // The pointed-to Constraint must remain valid until removeJointRef is called.
    void addJointRef(Constraint* joint);
    void removeJointRef(Constraint* joint);

    // Per-pair collision ignore. When ignore=true, contact generation between a and b
    // is suppressed regardless of collisionGroup/Mask. Symmetric: (a,b)==(b,a).
    // Ragdoll uses this for joint-connected pairs (1-hop) and near-chain pairs (2-hop).
    void setIgnoreCollision(RigidBody* a, RigidBody* b, bool ignore);

    // Register a static height-field terrain for body-terrain collision.
    // terrainBody must be MotionType::Static; it is NOT added to the broad phase.
    void registerTerrain(RigidBody* terrainBody, const TerrainHeightField* heightField);

    // Remove the terrain collider. Safe to call when no terrain is registered.
    void unregisterTerrain();

    // Register a body as a camera obstacle for queryCameraArm().
    // Call update() on the camera broad phase immediately, so subsequent
    // queryCameraArm() calls see the updated state.
    void registerCameraObstacle(RigidBody* body);
    void unregisterCameraObstacle(RigidBody* body);

    // Compute the maximum arm length from pivot toward desiredEye that
    // avoids registered terrain and camera obstacles.
    // spherePad expands obstacle AABBs and offsets hit distances.
    float MU_CALLCONV queryCameraArm(mu::Vec3 pivot, mu::Vec3 desiredEye, float spherePad) const;

    // Main simulation tick (integrate + detect + solve).
    void step(Seconds dt);

    // Set the gravitational acceleration applied to Dynamic bodies each step.
    // Default: {0, -9.8, 0} (world Y-up).
    void MU_CALLCONV setGravity(mu::Vec3 g) { gravity_ = g; }

    // Set the number of PGS velocity iterations per step (default: 10).
    void setSolverIterations(int n) { solverIterations_ = n; }

    // Extra velocity iterations applied only to persistent joint constraints.
    // Useful for ragdoll/chain stability without increasing contact cost.
    void setJointSolverExtraIterations(int n) { jointSolverExtraIterations_ = n; }

    // Set the number of Split Impulse position correction iterations (default: 3).
    // More iterations improve convergence for fast-moving Dynamic bodies.
    void setPositionSolveIterations(int n) { positionSolveIterations_ = n; }

    // Set the number of physics sub-steps per step() call (default: 2).
    // Each sub-step divides dt by n and runs integrate+detect+solve.
    // Higher values reduce steady-state penetration depth at the cost of CPU.
    void setSubStepCount(int n) { subStepCount_ = n; }

    // Iterate over all active contact constraints for the current step.
    // fn receives a const ContactConstraint& — call after step(), before the next step().
    template<typename Fn>
    void forEachContact(Fn&& fn) const {
        for (const auto& c : contactConstraints_)
            fn(*c);
    }

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
        uint16_t              collisionGroup = 1;
        uint16_t              collisionMask  = 0xFFFF;
    };

    std::vector<Entry>                          entries_;
    mu::Vec3                                    gravity_{ 0.f, -9.8f, 0.f };

    // Broad-phase collision detector (swappable; default BruteForceBroadPhase).
    std::unique_ptr<BroadPhase>                 broadPhase_;

    // Per-step contact constraints (rebuilt every step).
    std::vector<std::unique_ptr<ContactConstraint>> contactConstraints_;

    // Owned persistent constraints (added via addJointConstraint).
    std::vector<std::unique_ptr<Constraint>>    jointConstraints_;

    // Non-owning joint references (added via addJointRef, e.g. from Ragdoll).
    std::vector<Constraint*>                    jointRefs_;

    // Optional terrain collider (null when no terrain is registered).
    // Terrain body is NOT in entries_ or broadPhase_; TerrainCollider
    // iterates Dynamic bodies directly.
    std::unique_ptr<TerrainCollider>            terrainCollider_;
    const TerrainHeightField*                   terrainHF_ = nullptr;

    // Separate broad phase for camera obstacle queries (not part of physics sim).
    std::unique_ptr<BroadPhase>                 cameraBroadPhase_;

    static constexpr float kCameraMinGroundClearance = 0.15f;

    int     solverIterations_         = 4;
    int     jointSolverExtraIterations_ = 0;
    int     positionSolveIterations_  = 3;
    int     subStepCount_             = 2;
    Seconds currentSubDt_{}; // set at start of each sub-step; used by generateContacts()

    // Warm-starting cache: carries accumulated contact impulses across steps
    // so PGS starts from the previous solution instead of zero.
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

    // Body pairs excluded from contact generation regardless of group/mask.
    // Keys are normalized (smaller ptr first) via normKey().
    std::unordered_set<std::pair<RigidBody*, RigidBody*>, WarmPairHash> ignoreCollisionPairs_;
};

#endif // __PhysicsWorld_HPP
