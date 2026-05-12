#include "pch.hpp"
#include "physicsWorld.hpp"
#include "terrain.hpp"

// 수직(y축) 공기 저항 계수. linearDamping은 수평 지면 마찰에 사용되므로
// y축에는 별도의 작은 값을 적용한다.
// 종단 속도 = gravity / kAirDamping (예: 9.8 / 0.5 ≈ 19.6 m/s)
static constexpr float kAirDamping = 0.5f;

PhysicsWorld::PhysicsWorld()
    : broadPhase_(std::make_unique<SAPBroadPhase>())
    , cameraBroadPhase_(std::make_unique<SAPBroadPhase>())
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
    terrainHF_ = heightField;
}

void PhysicsWorld::unregisterTerrain()
{
    terrainCollider_.reset();
    terrainHF_ = nullptr;
}

void PhysicsWorld::registerCameraObstacle(RigidBody* body)
{
    cameraBroadPhase_->add(body);
    cameraBroadPhase_->update();
}

void PhysicsWorld::unregisterCameraObstacle(RigidBody* body)
{
    cameraBroadPhase_->remove(body);
    cameraBroadPhase_->update();
}

float PhysicsWorld::queryCameraArm(mu::Vec3 pivot, mu::Vec3 desiredEye, float spherePad) const
{
    const float armLen = (desiredEye - pivot).len();
    if (armLen < 1e-6f) return 0.f;
    const mu::Vec3 armDir = (desiredEye - pivot) * (1.0f / armLen);
    const Ray armRay{ pivot, armDir };
    float allowed = armLen;

    // Terrain: sample N=6 points along the arm, find the first below ground.
    if (terrainHF_ && terrainCollider_) {
        const mu::Vec3 origin = terrainCollider_->terrainBody()->pos();
        constexpr int N = 6;
        for (int i = 1; i <= N; ++i) {
            const float t  = static_cast<float>(i) / N;
            const mu::Vec3 p = pivot + armDir * (t * armLen);
            const float lx = p.x() - origin.x();
            const float lz = p.z() - origin.z();
            if (lx < 0.f || lx > terrainHF_->sizeX) continue;
            if (lz < 0.f || lz > terrainHF_->sizeZ) continue;
            const float groundY = origin.y() + terrainHF_->getHeightAt(lx, lz);
            if (p.y() < groundY + kCameraMinGroundClearance) {
                allowed = std::min(allowed, (t - 1.0f / N) * armLen);
                break;
            }
        }
    }

    // Obstacle broad phase: arm AABB expanded by spherePad.
    const mu::Vec3 armMin = min(pivot, desiredEye) - mu::Vec3(spherePad, spherePad, spherePad);
    const mu::Vec3 armMax = max(pivot, desiredEye) + mu::Vec3(spherePad, spherePad, spherePad);
    const AABB armAABB{
        .center = (armMin + armMax) * 0.5f,
        .size   = armMax - armMin
    };
    const auto candidates = cameraBroadPhase_->queryAABB(armAABB);

    // Obstacle narrow phase: BVH ray cast on each candidate.
    for (const auto* body : candidates) {
        const RayHit hit = RaycastBVH(body->worldBVH(), armRay);
        if (hit.hit)
            allowed = std::min(allowed, hit.t - spherePad);
    }

    return std::max(0.f, allowed);
}

void PhysicsWorld::step(Seconds dt)
{
    const int     n     = subStepCount_;
    const Seconds subDt = Seconds(dt.count() / n);

    // advanceState once per game step so prev/curr interpolation spans the full step.
    for (auto& e : entries_) e.body->advanceState();

    for (int s = 0; s < n; ++s) {
        currentSubDt_ = subDt;
        for (auto& e : entries_) e.body->clearPseudoVelocities();
        integrate(subDt);
        generateContacts();
        solveConstraints(subDt);
        for (auto& e : entries_) e.body->applyPseudoVelocity(subDt);
    }
}

void PhysicsWorld::integrate(Seconds dt)
{
    for (auto& e : entries_) {
        RigidBody& b = *e.body;

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

            // Upright correction: apply a restoring torque that pulls the body's
            // local Y-axis back toward world Y-up (gravity's reference direction).
            // cross(bodyUp, worldUp) points along the axis that rotates bodyUp onto
            // worldUp; its magnitude equals sin(tilt angle) so the correction is
            // proportional to how far the body has tilted.
            if (b.uprightStiffness() > 0.f) {
                const mu::Vec3 bodyUp   = b.orient().rotate(mu::Vec3(0.f, 1.f, 0.f));
                const mu::Vec3 torqDir  = mu::cross(bodyUp, mu::Vec3(0.f, 1.f, 0.f));
                b.setOmega(b.omega() + (torqDir * (b.uprightStiffness() * dtf)) * b.invInertiaWorld());
            }

            // Integrate position and orientation.
            b.setPos(b.pos() + b.linearVel() * dtf);
            const auto wq = mu::Quat(b.omega(), 0.f);
            auto newOrient = mu::Quat(b.orient()) + mu::Quat(b.orient()) * wq * 0.5f * dtf;
            b.setOrient(mu::NQuat{ newOrient });
            b.updateInertiaWorld();

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

        // Contact normal: from narrow-phase geometry (B→A convention, fixed in collision.cpp).
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

            // Look-ahead: for a falling body, extend the contact range by one
            // sub-step's worth of travel so the constraint catches the body just
            // before it would tunnel through the terrain surface.
            const float vy = body->linearVel().y();
            const float lookAhead = (vy < 0.f)
                ? std::min(0.15f, std::abs(vy) * currentSubDt_.count())
                : 0.f;

            std::vector<ContactPoint> contacts;
            contacts.reserve(4);
            const int cnt = terrainCollider_->generateContacts(*body, contacts, lookAhead);
            if (cnt == 0) continue;

            auto cc = std::make_unique<ContactConstraint>(body, terrainCollider_->terrainBody());
            cc->setExternalAccels(gravity_, mu::Vec3(0.f, 0.f, 0.f));
            cc->setTerrainContact(true);
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
        for (auto  c : jointRefs_)          fn(*c);
    };

    allConstraints([&](Constraint& c) { c.prepare(dt); });

    // Warm-start: apply previous step's accumulated impulses as initial guess.
    // Called after prepare() so rA/rB/tangent are fresh.
    for (auto& cc : contactConstraints_) {
        auto it = warmCache_.find(normKey(cc->bodyA, cc->bodyB));
        if (it != warmCache_.end()) {
            const auto& w = it->second;
            for (int i = 0; i < cc->count; ++i) {
                cc->contacts[i].accNormal     = w.accNormal;
                cc->contacts[i].accTangent[0] = w.accTangent[0];
                cc->contacts[i].accTangent[1] = w.accTangent[1];
            }
            cc->applyWarmStart();
        }
    }

    for (int iter = 0; iter < solverIterations_; ++iter)
        allConstraints([](Constraint& c) { c.solveVelocity(); });

    for (int iter = 0; iter < positionSolveIterations_; ++iter)
        allConstraints([](Constraint& c) { c.solvePosition(); });

    // Save accumulated impulses for next step's warm-start.
    warmCache_.clear();
    for (auto& cc : contactConstraints_) {
        if (cc->count == 0) continue;
        WarmEntry w;
        w.accNormal     = cc->contacts[0].accNormal;
        w.accTangent[0] = cc->contacts[0].accTangent[0];
        w.accTangent[1] = cc->contacts[0].accTangent[1];
        warmCache_[normKey(cc->bodyA, cc->bodyB)] = w;
    }
}

mu::Vec3 MU_CALLCONV PhysicsWorld::interpolatePos(const RigidBody& b, float t)
{
    return mu::lerp(b.prev().pos, b.curr().pos, t);
}

mu::NQuat MU_CALLCONV PhysicsWorld::interpolateOrient(const RigidBody& b, float t)
{
    return mu::slerp(b.prev().orient, b.curr().orient, t);
}
