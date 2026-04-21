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

void DBVTBroadPhase::add(RigidBody* body) {
    bodies_.push_back(body);

    Node* leaf = new Node();
    leaf->body = body;
    leaf->aabb = fatten(body->worldAABB());

    leafMap_[body] = leaf;
    insertLeaf(leaf);
}

void DBVTBroadPhase::remove(RigidBody* body) {
    auto it = leafMap_.find(body);
    if (it == leafMap_.end()) return;

    Node* leaf = it->second;
    removeLeaf(leaf);
    delete leaf;

    leafMap_.erase(it);
    bodies_.erase(std::remove(bodies_.begin(), bodies_.end(), body), bodies_.end());
}

void DBVTBroadPhase::update() {
    for (RigidBody* body : bodies_) {
        Node* leaf = leafMap_[body];
        updateLeaf(leaf, body->worldAABB());
    }
}

std::vector<BodyPair> DBVTBroadPhase::queryPairs() {
    std::vector<BodyPair> pairs;
    if (!root_) return pairs;

    std::stack<std::pair<Node*, Node*>> stack;
    stack.push({ root_, root_ });

    while (!stack.empty()) {
        auto [a, b] = stack.top();
        stack.pop();

        if (a == b) {
            if (!a->isLeaf()) {
                stack.push({ a->left, a->left });
                stack.push({ a->right, a->right });
                stack.push({ a->left, a->right });
            }
            continue;
        }

        if (!collides(a->aabb, b->aabb).hit)
            continue;

        if (a->isLeaf() && b->isLeaf()) {
            const bool aStatic = (a->body->motionType() == MotionType::Static);
            const bool bStatic = (b->body->motionType() == MotionType::Static);
            if (!(aStatic && bStatic))
                pairs.push_back({ a->body, b->body });
        } else if (a->isLeaf()) {
            stack.push({ a, b->left });
            stack.push({ a, b->right });
        } else if (b->isLeaf()) {
            stack.push({ a->left, b });
            stack.push({ a->right, b });
        } else {
            stack.push({ a->left, b->left });
            stack.push({ a->left, b->right });
            stack.push({ a->right, b->left });
            stack.push({ a->right, b->right });
        }
    }
    // std::cout << "done with " << pairs.size() << " pairs.\n";

    return pairs;
}

AABB DBVTBroadPhase::merge(const AABB& a, const AABB& b) {
    mu::Vec3 minA = a.center - a.size * 0.5f;
    mu::Vec3 maxA = a.center + a.size * 0.5f;
    mu::Vec3 minB = b.center - b.size * 0.5f;
    mu::Vec3 maxB = b.center + b.size * 0.5f;

    mu::Vec3 minC = min(minA, minB);
    mu::Vec3 maxC = max(maxA, maxB);

    AABB out;
    out.center = (minC + maxC) * 0.5f;
    out.size   = (maxC - minC);
    return out;
}

float DBVTBroadPhase::surfaceArea(const AABB& a) {
    mu::Vec3 s = a.size;
    return 2.0f * (s.x()*s.y() + s.y()*s.z() + s.z()*s.x());
}

AABB DBVTBroadPhase::fatten(const AABB& aabb) const {
    AABB out = aabb;
    out.size = aabb.size * (1.0f + fatMargin_);
    return out;
}

void DBVTBroadPhase::insertLeaf(Node* leaf) {
    if (!root_) {
        root_ = leaf;
        leaf->parent = nullptr;
        return;
    }

    Node* current = root_;
    while (!current->isLeaf()) {
        AABB merged = merge(current->aabb, leaf->aabb);
        float cost = surfaceArea(merged) - surfaceArea(current->aabb);

        float costLeft;
        {
            AABB m = merge(current->left->aabb, leaf->aabb);
            costLeft = current->left->isLeaf()
                ? surfaceArea(m)
                : surfaceArea(m) - surfaceArea(current->left->aabb);
        }

        float costRight;
        {
            AABB m = merge(current->right->aabb, leaf->aabb);
            costRight = current->right->isLeaf()
                ? surfaceArea(m)
                : surfaceArea(m) - surfaceArea(current->right->aabb);
        }

        if (cost < costLeft && cost < costRight)
            break;

        current = (costLeft < costRight) ? current->left : current->right;
    }

    Node* oldParent = current->parent;
    Node* newParent = new Node();
    newParent->parent = oldParent;
    newParent->aabb = merge(current->aabb, leaf->aabb);
    newParent->left = current;
    newParent->right = leaf;

    current->parent = newParent;
    leaf->parent = newParent;

    if (!oldParent) {
        root_ = newParent;
    } else {
        if (oldParent->left == current) oldParent->left = newParent;
        else oldParent->right = newParent;
    }

    // refit
    Node* p = leaf->parent;
    while (p) {
        p->aabb = merge(p->left->aabb, p->right->aabb);
        p = p->parent;
    }
}

void DBVTBroadPhase::removeLeaf(Node* leaf) {
    if (leaf == root_) {
        root_ = nullptr;
        return;
    }

    Node* parent = leaf->parent;
    Node* grand = parent->parent;
    Node* sibling = (parent->left == leaf) ? parent->right : parent->left;

    if (!grand) {
        root_ = sibling;
        sibling->parent = nullptr;
    } else {
        if (grand->left == parent) grand->left = sibling;
        else grand->right = sibling;
        sibling->parent = grand;

        Node* p = grand;
        while (p) {
            p->aabb = merge(p->left->aabb, p->right->aabb);
            p = p->parent;
        }
    }

    delete parent;
}

void DBVTBroadPhase::updateLeaf(Node* leaf, const AABB& newAABB) {
    if (collides(leaf->aabb, newAABB).hit)
        return; // fat AABB 안에 있음

    removeLeaf(leaf);
    leaf->aabb = fatten(newAABB);
    insertLeaf(leaf);
}