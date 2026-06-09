#ifndef room_server_broadPhase_hpp
#define room_server_broadPhase_hpp

#include "rigidBody.hpp"

// A potentially colliding pair of bodies produced by the broad phase.
struct BodyPair {
    RigidBody* a;
    RigidBody* b;
};

// Interface for the broad-phase collision detection stage.
class BroadPhase {
public:
    virtual ~BroadPhase() = default;

    virtual void add(RigidBody* body) = 0;
    virtual void remove(RigidBody* body) = 0;
    virtual void update() = 0;
    virtual std::vector<BodyPair> queryPairs() = 0;
};

// O(n^2) reference implementation.
class BruteForceBroadPhase : public BroadPhase {
public:
    void add(RigidBody* body) override;
    void remove(RigidBody* body) override;
    void update() override {}
    std::vector<BodyPair> queryPairs() override;

private:
    std::vector<RigidBody*> bodies_;
};

// Sort-and-Sweep broad phase along the X axis. O(n log n).
class SAPBroadPhase : public BroadPhase {
public:
    void add(RigidBody* body) override;
    void remove(RigidBody* body) override;
    void update() override;
    std::vector<BodyPair> queryPairs() override;

    // Remove all bodies (membership rebuilt per frame by some callers, e.g. the
    // NPC separation broad phase). Keeps allocated capacity for reuse.
    void clear() { bodies_.clear(); endpoints_.clear(); }

    // Fatten every body's AABB by this margin on each side before overlap tests.
    // Default 0 leaves behavior identical (physics instance). A non-zero margin
    // turns the broad phase into a radius-style neighbor query: bodies within
    // ~margin of each other are reported as candidate pairs.
    void setFatMargin(float m) { fatMargin_ = m; }

private:
    struct Endpoint {
        float      value;
        RigidBody* body;
        bool       isMax;
    };

    std::vector<RigidBody*> bodies_;
    std::vector<Endpoint>   endpoints_;
    float                   fatMargin_ = 0.f;

    static bool overlapYZ(const AABB& a, const AABB& b, float margin);
};

#endif // room_server_broadPhase_hpp
