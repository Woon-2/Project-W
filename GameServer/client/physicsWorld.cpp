#include "pch.hpp"
#include "physicsWorld.hpp"
#include "terrain.hpp"

// 수직(y축) 공기 저항 계수. linearDamping은 수평 지면 마찰에 사용되므로
// y축에는 별도의 작은 값을 적용한다.
// 종단 속도 = gravity / kAirDamping (예: 9.8 / 0.5 ≈ 19.6 m/s)
static constexpr float kAirDamping = 0.5f;

PhysicsWorld::PhysicsWorld()
    : broadPhase_(std::make_unique<SAPBroadPhase>())
{}

void PhysicsWorld::registerBody(RigidBody* body,
                                std::function<void()> onRebuildBVH,
                                uint16_t collisionGroup,
                                uint16_t collisionMask)
{
    entries_.push_back({ body, std::move(onRebuildBVH), collisionGroup, collisionMask });
    broadPhase_->add(body);
}

void PhysicsWorld::unregisterBody(RigidBody* body)
{
    auto it = std::ranges::find(entries_, body, &Entry::body);
    if (it != entries_.end())
        entries_.erase(it);
    broadPhase_->remove(body);
}

void PhysicsWorld::addJointConstraint(std::unique_ptr<Constraint> joint)
{
    jointConstraints_.push_back(std::move(joint));
}

void PhysicsWorld::removeJointConstraint(Constraint* joint)
{
    auto it = std::ranges::find_if(jointConstraints_,
        [joint](const auto& p) { return p.get() == joint; });
    if (it != jointConstraints_.end())
        jointConstraints_.erase(it);
}

void PhysicsWorld::addJointRef(Constraint* joint)
{
    jointRefs_.push_back(joint);
}

void PhysicsWorld::removeJointRef(Constraint* joint)
{
    auto it = std::ranges::find(jointRefs_, joint);
    if (it != jointRefs_.end())
        jointRefs_.erase(it);
}

void PhysicsWorld::registerTerrain(RigidBody* terrainBody,
                                    const TerrainHeightField* heightField)
{
    terrainCollider_ = std::make_unique<TerrainCollider>(terrainBody, heightField);
}

void PhysicsWorld::unregisterTerrain()
{
    terrainCollider_.reset();
}

void PhysicsWorld::step(Seconds dt)
{
    integrate(dt);        // advance state, apply forces, rebuild BVHs
    generateContacts();   // broad phase -> narrow phase -> ContactConstraints
    solveConstraints(dt); // PGS: prepare, iterate velocity solve, position solve
}

void PhysicsWorld::integrate(Seconds dt)
{
    for (auto& e : entries_) {
        RigidBody& b = *e.body;

        // Advance double buffer so prev holds the state from before this tick.
        b.advanceState();

        switch (b.motionType()) {
        case MotionType::Kinematic:
        {
            const float dtf = dt.count();

            // Ground friction: damp velocity so game logic only needs to handle
            // acceleration. linearDamping_ == 0 disables friction entirely.
            b.setLinearVel(b.linearVel() * std::max(0.f, 1.f - b.linearDamping()  * dtf));
            b.setOmega    (b.omega()     * std::max(0.f, 1.f - b.angularDamping() * dtf));

            // Snap tiny velocities to zero to prevent infinite-creep artefacts.
            if (b.linearVel().len2() < 1e-4f) b.setLinearVel(mu::Vec3{});
            if (b.omega().len2()     < 1e-4f) b.setOmega(mu::Vec3{});

            // Integrate position and orientation.
            b.setPos(b.pos() + b.linearVel() * dtf);

            // Quaternion integration: dq = q * [omega, 0] * 0.5 * dt
            const auto wq = mu::Quat(b.omega(), 0.f);
            auto newOrient = mu::Quat(b.orient()) + mu::Quat(b.orient()) * wq * 0.5f * dtf;
            b.setOrient(mu::NQuat{ newOrient });
            break;
        }
        case MotionType::Dynamic:
        {
            if (b.invMass() <= 0.f) break;  // guard: treat as Static if mass unset

            const float dtf = dt.count();

            // Velocity damping (applied first so it does not damp the new impulse).
            // x/z: ground friction (linearDamping). y: air resistance (kAirDamping).
            {
                const auto vel = b.linearVel();
                const float horzDamp = std::max(0.f, 1.f - b.linearDamping() * dtf);
                const float vertDamp = std::max(0.f, 1.f - kAirDamping * dtf);
                b.setLinearVel(mu::Vec3(vel.x() * horzDamp, vel.y() * vertDamp, vel.z() * horzDamp));
            }
            b.setOmega(b.omega() * std::max(0.f, 1.f - b.angularDamping() * dtf));

            const auto linAcc = gravity_ + b.forceAccum() * b.invMass();
            b.setLinearVel(b.linearVel() + linAcc * dtf);

            // Integrate angular velocity: I_world^-1 * torque.
            // In DirectXMath row-vector form: torque * invInertiaWorld.
            b.updateInertiaWorld();
            const auto angAcc = b.torqueAccum() * b.invInertiaWorld();
            b.setOmega(b.omega() + angAcc * dtf);

            // Integrate position and orientation.
            b.setPos(b.pos() + b.linearVel() * dtf);
            const auto wq = mu::Quat(b.omega(), 0.f);
            auto newOrient = mu::Quat(b.orient()) + mu::Quat(b.orient()) * wq * 0.5f * dtf;
            b.setOrient(mu::NQuat{ newOrient });

            b.clearAccumulators();
            break;
        }
        case MotionType::Static:
            // Never moves.
            break;
        }

        // Rebuild world-space BVH after position/orientation changed.
        if (e.onRebuildBVH)
            e.onRebuildBVH();
    }
}

void PhysicsWorld::generateContacts()
{
    // Discard contacts from the previous step.
    contactConstraints_.clear();

    broadPhase_->update();
    const auto pairs = broadPhase_->queryPairs();

    for (const auto& [a, b] : pairs) {
        // Collision group/mask filter.
        auto findEntry = [&](RigidBody* body) -> const Entry* {
            auto it = std::ranges::find(entries_, body, &Entry::body);
            return (it != entries_.end()) ? &*it : nullptr;
        };
        const Entry* ea = findEntry(a);
        const Entry* eb = findEntry(b);
        if (ea && eb) {
            const bool ab = (ea->collisionGroup & eb->collisionMask) != 0;
            const bool ba = (eb->collisionGroup & ea->collisionMask) != 0;
            if (!ab && !ba) continue;
        }

        const BVH& bvhA = a->worldBVH();
        const BVH& bvhB = b->worldBVH();

        // Skip if either body has no collision shape.
        if (bvhA.empty() || bvhB.empty()) continue;

        const CollisionResult res = collides(bvhA, bvhB);
        if (!res.hit) continue;

        auto cc = std::make_unique<ContactConstraint>(a, b);

        // Contact normal: direction B-toward-A (A separates in +normal direction).
        // We derive this from the center-to-center vector for reliability, since
        // the collision.cpp mtv convention may differ between AABB and OBB shapes.
        mu::Vec3 sep = a->pos() - b->pos();
        const float sepLen2 = sep.len2();
        mu::NVec3 normal = (sepLen2 > 1e-6f)
            ? mu::normalize(sep)
            : mu::NVec3(0.f, 1.f, 0.f, mu::NVec3::NoNormalize_t{});

        ContactPoint cp;
        cp.worldPos = res.contactPoint;
        cp.normal   = normal;
        cp.depth    = res.depth;
        cp.localA   = res.contactPoint - a->pos();
        cp.localB   = res.contactPoint - b->pos();
        cc->addContact(cp);

        contactConstraints_.push_back(std::move(cc));
    }

    // --- Body-Terrain contacts ---
    if (terrainCollider_) {
        for (auto& e : entries_) {
            RigidBody* body = e.body;
            if (body->motionType() != MotionType::Dynamic) continue;
            if (body->worldBVH().empty()) continue;

            std::vector<ContactPoint> contacts;
            contacts.reserve(4);
            const int cnt = terrainCollider_->generateContacts(*body, contacts);
            if (cnt == 0) continue;

            auto cc = std::make_unique<ContactConstraint>(body, terrainCollider_->terrainBody());
            for (auto& cp : contacts) {
                cp.localA = cp.worldPos - body->pos();
                cp.localB = cp.worldPos - terrainCollider_->terrainBody()->pos();
                cc->addContact(cp);
            }
            contactConstraints_.push_back(std::move(cc));
        }
    }
}

void PhysicsWorld::solveConstraints(Seconds dt)
{
    // Gather all constraints (contacts first, then persistent joints).
    auto allConstraints = [&](auto fn) {
        for (auto& c : contactConstraints_) fn(*c);
        for (auto& c : jointConstraints_)   fn(*c);
        for (auto  c : jointRefs_)          fn(*c);
    };

    // prepare() caches effective masses and biases.
    allConstraints([&](Constraint& c) { c.prepare(dt); });

    // PGS velocity iterations.
    for (int iter = 0; iter < solverIterations_; ++iter)
        allConstraints([](Constraint& c) { c.solveVelocity(); });

    // Position-level correction (split impulse or no-op for Baumgarte-only).
    allConstraints([](Constraint& c) { c.solvePosition(); });
}

mu::Vec3 MU_CALLCONV PhysicsWorld::interpolatePos(const RigidBody& b, float t)
{
    return mu::lerp(b.prev().pos, b.curr().pos, t);
}

mu::NQuat MU_CALLCONV PhysicsWorld::interpolateOrient(const RigidBody& b, float t)
{
    return mu::slerp(b.prev().orient, b.curr().orient, t);
}
