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

private:
    struct Endpoint {
        float      value;
        RigidBody* body;
        bool       isMax;
    };

    std::vector<RigidBody*> bodies_;
    std::vector<Endpoint>   endpoints_;

    static bool overlapYZ(const AABB& a, const AABB& b);
};

#endif // room_server_broadPhase_hpp
