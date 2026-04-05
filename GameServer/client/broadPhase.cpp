#include "pch.hpp"
#include "broadPhase.hpp"

void BruteForceBroadPhase::add(RigidBody* body)
{
    bodies_.push_back(body);
}

void BruteForceBroadPhase::remove(RigidBody* body)
{
    auto it = std::ranges::find(bodies_, body);
    if (it != bodies_.end())
        bodies_.erase(it);
}

std::vector<BodyPair> BruteForceBroadPhase::queryPairs()
{
    std::vector<BodyPair> pairs;

    for (std::size_t i = 0; i < bodies_.size(); ++i) {
        for (std::size_t j = i + 1; j < bodies_.size(); ++j) {
            RigidBody* a = bodies_[i];
            RigidBody* b = bodies_[j];

            // Skip Static-Static: neither can be moved by impulses.
            const bool aStatic = (a->motionType() == MotionType::Static);
            const bool bStatic = (b->motionType() == MotionType::Static);
            if (aStatic && bStatic) continue;

            // AABB overlap test using each body's root BVH bounds.
            const AABB aabbA = a->worldAABB();
            const AABB aabbB = b->worldAABB();
            if (!collides(aabbA, aabbB).hit) continue;

            pairs.push_back({ a, b });
        }
    }

    return pairs;
}
