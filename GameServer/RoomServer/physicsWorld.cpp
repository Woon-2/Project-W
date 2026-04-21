#include "rspch.hpp"
#include "physicsWorld.hpp"

// Vertical air resistance coefficient (y-axis only).
// linearDamping is used for horizontal ground friction.
static constexpr float kAirDamping = 0.5f;

PhysicsWorld::PhysicsWorld()
    : broadPhase_(std::make_unique<SAPBroadPhase>())
{}

void PhysicsWorld::registerBody(RigidBody* body,
                                std::function<void()> onRebuildBVH)
{
    entries_.push_back({ body, std::move(onRebuildBVH) });
    broadPhase_->add(body);
}

void PhysicsWorld::registerTerrain(RigidBody* terrainBody, const TerrainHeightField* hf)
{
    terrainCollider_ = std::make_unique<TerrainCollider>(terrainBody, hf);
}

void PhysicsWorld::unregisterTerrain()
{
    terrainCollider_.reset();
}

void PhysicsWorld::unregisterBody(RigidBody* body)
{
    auto it = std::ranges::find(entries_, body, &Entry::body);
    if (it != entries_.end())
        entries_.erase(it);
    broadPhase_->remove(body);
}

void PhysicsWorld::step(Seconds dt)
{
    integrate(dt);
    generateContacts();
    solveConstraints(dt);
}

void PhysicsWorld::integrate(Seconds dt)
{
    for (auto& e : entries_) {
        RigidBody& b = *e.body;

        b.advanceState();

        switch (b.motionType()) {
        case MotionType::Kinematic:
        {
            const float dtf = dt.count();

            b.setLinearVel(b.linearVel() * std::max(0.f, 1.f - b.linearDamping()  * dtf));
            b.setOmega    (b.omega()     * std::max(0.f, 1.f - b.angularDamping() * dtf));

            if (b.linearVel().len2() < 1e-4f) b.setLinearVel(mu::Vec3{});
            if (b.omega().len2()     < 1e-4f) b.setOmega(mu::Vec3{});

            b.setPos(b.pos() + b.linearVel() * dtf);

            const auto wq = mu::Quat(b.omega(), 0.f);
            auto newOrient = mu::Quat(b.orient()) + mu::Quat(b.orient()) * wq * 0.5f * dtf;
            b.setOrient(mu::NQuat{ newOrient });
            break;
        }
        case MotionType::Dynamic:
        {
            if (b.invMass() <= 0.f) break;

            const float dtf = dt.count();

            {
                const auto vel = b.linearVel();
                const float horzDamp = std::max(0.f, 1.f - b.linearDamping() * dtf);
                const float vertDamp = std::max(0.f, 1.f - kAirDamping * dtf);
                b.setLinearVel(mu::Vec3(vel.x() * horzDamp, vel.y() * vertDamp, vel.z() * horzDamp));
            }
            b.setOmega(b.omega() * std::max(0.f, 1.f - b.angularDamping() * dtf));

            const auto linAcc = gravity_ + b.forceAccum() * b.invMass();
            b.setLinearVel(b.linearVel() + linAcc * dtf);

            b.updateInertiaWorld();
            const auto angAcc = b.torqueAccum() * b.invInertiaWorld();
            b.setOmega(b.omega() + angAcc * dtf);

            b.setPos(b.pos() + b.linearVel() * dtf);
            const auto wq = mu::Quat(b.omega(), 0.f);
            auto newOrient = mu::Quat(b.orient()) + mu::Quat(b.orient()) * wq * 0.5f * dtf;
            b.setOrient(mu::NQuat{ newOrient });

            b.clearAccumulators();
            break;
        }
        case MotionType::Static:
            break;
        }

        if (e.onRebuildBVH)
            e.onRebuildBVH();
    }
}

void PhysicsWorld::generateContacts()
{
    contactConstraints_.clear();

    broadPhase_->update();
    const auto pairs = broadPhase_->queryPairs();

    for (const auto& [a, b] : pairs) {
        const BVH& bvhA = a->worldBVH();
        const BVH& bvhB = b->worldBVH();

        if (bvhA.empty() || bvhB.empty()) continue;

        const CollisionResult res = collides(bvhA, bvhB);
        if (!res.hit) continue;

        auto cc = std::make_unique<ContactConstraint>(a, b);

        // Contact normal: from narrow-phase geometry (B°ÊA convention, fixed in collision.cpp).
        // Fall back to center-to-center only when the narrow-phase normal is degenerate.
        mu::NVec3 normal = res.normal;
        if (mu::Vec3(normal).len2() < 0.5f) {
            const mu::Vec3 sep = a->pos() - b->pos();
            const float sepLen2 = sep.len2();
            normal = (sepLen2 > 1e-6f)
                ? mu::normalize(sep)
                : mu::NVec3(0.f, 1.f, 0.f, mu::NVec3::NoNormalize_t{});
        }

        ContactPoint cp;
        cp.worldPos = res.contactPoint;
        cp.normal   = normal;
        cp.depth    = res.depth;
        cp.localA   = res.contactPoint - a->pos();
        cp.localB   = res.contactPoint - b->pos();
        cc->addContact(cp);

        const mu::Vec3 gravA = (a->motionType() == MotionType::Dynamic) ? gravity_ : mu::Vec3(0.f, 0.f, 0.f);
        const mu::Vec3 gravB = (b->motionType() == MotionType::Dynamic) ? gravity_ : mu::Vec3(0.f, 0.f, 0.f);
        cc->setExternalAccels(gravA, gravB);

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
            cc->setExternalAccels(gravity_, mu::Vec3(0.f, 0.f, 0.f));

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
    auto allConstraints = [&](auto fn) {
        for (auto& c : contactConstraints_) fn(*c);
        for (auto& c : jointConstraints_)   fn(*c);
    };

    allConstraints([&](Constraint& c) { c.prepare(dt); });

    for (int iter = 0; iter < solverIterations_; ++iter)
        allConstraints([](Constraint& c) { c.solveVelocity(); });

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
