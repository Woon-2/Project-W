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

// ---------------------------------------------------------------------------
// SAPBroadPhase
// ---------------------------------------------------------------------------

void SAPBroadPhase::add(RigidBody* body)
{
    bodies_.push_back(body);
    // endpoints_ is fully rebuilt in update(), so no incremental insert needed.
}

void SAPBroadPhase::remove(RigidBody* body)
{
    auto it = std::ranges::find(bodies_, body);
    if (it != bodies_.end())
        bodies_.erase(it);
}

void SAPBroadPhase::update()
{
    endpoints_.clear();
    endpoints_.reserve(bodies_.size() * 2);

    for (RigidBody* body : bodies_) {
        const AABB aabb  = body->worldAABB();
        const float minX = aabb.center.x() - aabb.size.x() * 0.5f;
        const float maxX = aabb.center.x() + aabb.size.x() * 0.5f;
        endpoints_.push_back({ minX, body, false });
        endpoints_.push_back({ maxX, body, true  });
    }

    // Insertion sort. The endpoint list is nearly sorted from the previous frame
    // (bodies move little between steps), so this is O(n) in the typical case.
    for (int i = 1; i < static_cast<int>(endpoints_.size()); ++i) {
        const Endpoint key = endpoints_[i];
        int j = i - 1;
        while (j >= 0 && endpoints_[j].value > key.value) {
            endpoints_[j + 1] = endpoints_[j];
            --j;
        }
        endpoints_[j + 1] = key;
    }
}

bool SAPBroadPhase::overlapYZ(const AABB& a, const AABB& b)
{
    const float aMinY = a.center.y() - a.size.y() * 0.5f;
    const float aMaxY = a.center.y() + a.size.y() * 0.5f;
    const float bMinY = b.center.y() - b.size.y() * 0.5f;
    const float bMaxY = b.center.y() + b.size.y() * 0.5f;
    if (aMaxY < bMinY || bMaxY < aMinY) return false;

    const float aMinZ = a.center.z() - a.size.z() * 0.5f;
    const float aMaxZ = a.center.z() + a.size.z() * 0.5f;
    const float bMinZ = b.center.z() - b.size.z() * 0.5f;
    const float bMaxZ = b.center.z() + b.size.z() * 0.5f;
    if (aMaxZ < bMinZ || bMaxZ < aMinZ) return false;

    return true;
}

std::vector<BodyPair> SAPBroadPhase::queryPairs()
{
    std::vector<BodyPair> pairs;

    // Active set: bodies whose min endpoint has been seen but max has not yet.
    // X overlap is guaranteed for any body in the active set when a new min is hit.
    std::vector<RigidBody*> active;
    active.reserve(16);

    for (const Endpoint& ep : endpoints_) {
        if (!ep.isMax) {
            // Min endpoint: check against all currently active bodies.
            for (RigidBody* other : active) {
                const bool aStatic = (ep.body->motionType() == MotionType::Static);
                const bool bStatic = (other->motionType()   == MotionType::Static);
                if (aStatic && bStatic) continue;

                // X overlap is guaranteed; verify Y and Z.
                if (overlapYZ(ep.body->worldAABB(), other->worldAABB()))
                    pairs.push_back({ ep.body, other });
            }
            active.push_back(ep.body);
        } else {
            // Max endpoint: remove body from active set.
            auto it = std::ranges::find(active, ep.body);
            if (it != active.end())
                active.erase(it);
        }
    }

    return pairs;
}
