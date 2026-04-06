#ifndef __BroadPhase_HPP
#define __BroadPhase_HPP

#include "rigidBody.hpp"

// A potentially colliding pair of bodies produced by the broad phase.
struct BodyPair {
    RigidBody* a;
    RigidBody* b;
};

// Interface for the broad-phase collision detection stage.
// Implementations are swappable in PhysicsWorld (default: BruteForceBroadPhase).
//
// Lifecycle (called once per PhysicsWorld::step):
//   update()      - refresh internal data structures from current body positions
//   queryPairs()  - return pairs whose AABBs overlap (false positives allowed)
class BroadPhase {
public:
    virtual ~BroadPhase() = default;

    // Register a body for broad-phase tracking.
    virtual void add(RigidBody* body) = 0;

    // Unregister a body. No-op if body is not registered.
    virtual void remove(RigidBody* body) = 0;

    // Refresh internal data (re-sort, re-index, etc.) based on current body AABBs.
    // Must be called before queryPairs() each step.
    virtual void update() = 0;

    // Return all pairs whose world AABBs overlap.
    // May include false positives; the narrow phase filters them.
    // Static-Static pairs are never returned.
    virtual std::vector<BodyPair> queryPairs() = 0;
};

// O(n^2) reference implementation.
// Suitable for small scenes (< ~50 dynamic bodies).
class BruteForceBroadPhase : public BroadPhase {
public:
    void add(RigidBody* body) override;
    void remove(RigidBody* body) override;
    void update() override {}   // no data structure to update
    std::vector<BodyPair> queryPairs() override;

private:
    std::vector<RigidBody*> bodies_;
};

// Sort-and-Sweep broad phase along the X axis.
// Complexity: O(n log n) sort + O(n + k) sweep per frame, where k = output pairs.
// Uses insertion sort to exploit temporal coherence (body positions change little
// between frames, keeping the endpoint list nearly sorted).
class SAPBroadPhase : public BroadPhase {
public:
    void add(RigidBody* body) override;
    void remove(RigidBody* body) override;

    // Rebuilds endpoint list from current body AABBs and insertion-sorts it.
    void update() override;

    // Sweeps the sorted endpoint list with an active set, then checks Y/Z overlap.
    // Static-Static pairs are never returned.
    std::vector<BodyPair> queryPairs() override;

private:
    struct Endpoint {
        float      value;  // AABB min or max on X axis
        RigidBody* body;
        bool       isMax;  // false = min endpoint, true = max endpoint
    };

    std::vector<RigidBody*> bodies_;
    std::vector<Endpoint>   endpoints_; // sorted by value after update()

    static bool overlapYZ(const AABB& a, const AABB& b);
};

#endif // __BroadPhase_HPP
