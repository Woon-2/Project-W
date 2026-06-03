#include "pch.hpp"
#include "physicsWorld.hpp"
#include "terrain.hpp"

// 수직(y축) 공기 저항 계수. linearDamping은 수평 지면 마찰에 사용되므로
// y축에는 별도의 작은 값을 적용한다.
// 종단 속도 = gravity / kAirDamping (예: 9.8 / 0.15 ≈ 65 m/s).
// 값이 작을수록 종단 속도가 커지고 시상수(τ = 1/kAirDamping)도 길어져
// 낙하 초반(체감 구간)이 현실의 자유낙하 g·t에 가까워진다.
static constexpr float kAirDamping = 0.15f;

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

void PhysicsWorld::setIgnoreCollision(RigidBody* a, RigidBody* b, bool ignore)
{
    const auto key = normKey(a, b);
    if (ignore) ignoreCollisionPairs_.insert(key);
    else        ignoreCollisionPairs_.erase(key);
}

PhysicsWorld::TerrainHandle PhysicsWorld::registerTerrain(RigidBody* terrainBody,
                                    const TerrainHeightField* heightField)
{
    std::size_t slot;
    if (!freeTerrainSlots_.empty()) {
        slot = freeTerrainSlots_.back();
        freeTerrainSlots_.pop_back();
    } else {
        slot = terrains_.size();
        terrains_.emplace_back();
    }
    terrains_[slot].collider = std::make_unique<TerrainCollider>(terrainBody, heightField);
    terrains_[slot].hf       = heightField;
    return static_cast<TerrainHandle>(slot);
}

void PhysicsWorld::unregisterTerrain(TerrainHandle handle)
{
    if (handle == kInvalidTerrainHandle || handle >= terrains_.size()) return;
    if (!terrains_[handle].collider) return;   // already inactive
    terrains_[handle].collider.reset();
    terrains_[handle].hf = nullptr;
    freeTerrainSlots_.push_back(handle);
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
    // Each sample is routed to whichever registered chunk contains its XZ.
    if (!terrains_.empty()) {
        constexpr int N = 6;
        for (int i = 1; i <= N; ++i) {
            const float t  = static_cast<float>(i) / N;
            const mu::Vec3 p = pivot + armDir * (t * armLen);
            for (const auto& te : terrains_) {
                if (!te.collider || !te.hf) continue;
                const mu::Vec3 origin = te.collider->terrainBody()->pos();
                const float lx = p.x() - origin.x();
                const float lz = p.z() - origin.z();
                if (lx < 0.f || lx > te.hf->sizeX) continue;
                if (lz < 0.f || lz > te.hf->sizeZ) continue;
                const float groundY = origin.y() + te.hf->getHeightAt(lx, lz);
                if (p.y() < groundY + kCameraMinGroundClearance) {
                    allowed = std::min(allowed, (t - 1.0f / N) * armLen);
                }
                break;  // sample lies in exactly one chunk
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

        // Static contacts get the final word on position: resolved after the
        // dynamic-dynamic solver so a body squeezed against a wall still ends up
        // outside it. resolveStaticPenetration() writes curr_.pos directly, so
        // the BVH built during integrate() is stale for moved bodies; rebuild
        // only those (dirty set) so the next sub-step's narrow phase sees the
        // corrected pose.
        movedByStaticDepen_.clear();
        resolveStaticPenetration(staticContacts_, movedByStaticDepen_);
        for (RigidBody* b : movedByStaticDepen_) {
            auto it = std::ranges::find(entries_, b, &Entry::body);
            if (it != entries_.end() && it->onRebuildBVH)
                it->onRebuildBVH();
        }
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

            // Clamp angular speed to prevent runaway spin (e.g. from unsolved joint chains).
            {
                static constexpr float kMaxAngularSpeed = 50.f;
                const float omegaLen2 = b.omega().len2();
                if (omegaLen2 > kMaxAngularSpeed * kMaxAngularSpeed)
                    b.setOmega(b.omega() * (kMaxAngularSpeed / std::sqrt(omegaLen2)));
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
    staticContacts_.clear();

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

        // Per-pair ignore (ragdoll joint-connected and near-chain pairs).
        if (ignoreCollisionPairs_.count(normKey(a, b))) continue;

        const BVH& bvhA = a->worldBVH();
        const BVH& bvhB = b->worldBVH();

        // Skip if either body has no collision shape.
        if (bvhA.empty() || bvhB.empty()) continue;

        const CollisionResult res = collides(bvhA, bvhB);
        if (!res.hit) continue;

        // Static-Dynamic / Static-Kinematic: route to the lightweight
        // depenetration path instead of the ContactConstraint solver. Static's
        // infinite mass makes the impulse solver wasteful and prone to spurious
        // spin / launch; positional depenetration pushes the movable body out by
        // the penetration amount only. (Static-Static is already filtered by the
        // broad phase, so this branch sees exactly one static body.)
        const bool aStatic = a->motionType() == MotionType::Static;
        const bool bStatic = b->motionType() == MotionType::Static;
        if (aStatic != bStatic) {
            RigidBody* movable    = aStatic ? b : a;
            RigidBody* staticBody = aStatic ? a : b;

            // Normal points static -> movable (push-out). res.normal is B->A;
            // it already points static->movable when movable==a, and must be
            // flipped when movable==b. Fall back to center-to-center only when
            // the narrow-phase normal is degenerate.
            mu::NVec3 n = res.normal;
            if (mu::Vec3(n).len2() < 0.5f) {
                const mu::Vec3 sep = movable->pos() - staticBody->pos();
                n = (sep.len2() > 1e-6f)
                    ? mu::normalize(sep)
                    : mu::NVec3(0.f, 1.f, 0.f, mu::NVec3::NoNormalize_t{});
            } else if (movable == b) {
                // Negate in place; already unit length, so skip re-normalization.
                n = mu::NVec3(-mu::Vec3(n), mu::NVec3::NoNormalize_t{});
            }

            StaticContact sc;
            sc.movable    = movable;
            sc.staticBody = staticBody;
            ContactPoint cp;
            cp.worldPos = res.contactPoint;
            cp.normal   = n;            // static -> movable
            cp.depth    = res.depth;
            cp.localA   = res.contactPoint - movable->pos();
            sc.addContact(cp);
            staticContacts_.push_back(std::move(sc));
            continue;                   // do NOT build a ContactConstraint
        }

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

        // Character-character separation is horizontal-only. Baumgarte injects REAL
        // separating velocity along the contact normal, so a +Y-tilted normal (two
        // upright characters whose bone-boxes touch at different heights) launches
        // them upward. Project the normal and penetration onto the horizontal plane
        // for non-Static pairs (agents). Static obstacles (stronghold) keep their true
        // normal so bodies are blocked by / rest on them. Terrain is a separate path.
        float depth = res.depth;
        if (a->motionType() != MotionType::Static && b->motionType() != MotionType::Static) {
            const mu::Vec3 n3 = mu::Vec3(normal);
            const mu::Vec3 horiz(n3.x(), 0.f, n3.z());
            const float horizLen = horiz.len();
            if (horizLen > 1e-3f) {
                normal = mu::normalize(horiz);
                depth *= horizLen;            // horizontal component of the MTV
            } else {
                // Near-vertical normal (rare): use horizontal center-to-center.
                const mu::Vec3 sepXZ(a->pos().x() - b->pos().x(), 0.f, a->pos().z() - b->pos().z());
                if (sepXZ.len2() > 1e-6f)
                    normal = mu::normalize(sepXZ);
            }
        }

        ContactPoint cp;
        cp.worldPos = res.contactPoint;
        cp.normal   = normal;
        cp.depth    = depth;
        cp.localA   = res.contactPoint - a->pos();
        cp.localB   = res.contactPoint - b->pos();
        cc->addContact(cp);

        const mu::Vec3 gravA = (a->motionType() == MotionType::Dynamic) ? gravity_ : mu::Vec3(0.f, 0.f, 0.f);
        const mu::Vec3 gravB = (b->motionType() == MotionType::Dynamic) ? gravity_ : mu::Vec3(0.f, 0.f, 0.f);
        cc->setExternalAccels(gravA, gravB);

        contactConstraints_.push_back(std::move(cc));
    }

    // --- Body-Terrain contacts ---
    // For each dynamic body, test against every registered chunk. Each collider
    // self-rejects vertices outside its XZ footprint, so a body straddling a chunk
    // boundary correctly gets contacts from both chunks.
    if (!terrains_.empty()) {
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

            for (auto& te : terrains_) {
                if (!te.collider) continue;

                // Cheap XZ-footprint reject before walking the body's BVH leaves.
                const mu::Vec3 origin = te.collider->terrainBody()->pos();
                const mu::Vec3 bp = body->pos();
                constexpr float kPad = 4.f;   // generous: body half-extent + look-ahead
                if (te.hf) {
                    if (bp.x() < origin.x() - kPad || bp.x() > origin.x() + te.hf->sizeX + kPad) continue;
                    if (bp.z() < origin.z() - kPad || bp.z() > origin.z() + te.hf->sizeZ + kPad) continue;
                }

                std::vector<ContactPoint> contacts;
                contacts.reserve(4);
                const int cnt = te.collider->generateContacts(*body, contacts, lookAhead);
                if (cnt == 0) continue;

                auto cc = std::make_unique<ContactConstraint>(body, te.collider->terrainBody());
                cc->setExternalAccels(gravity_, mu::Vec3(0.f, 0.f, 0.f));
                cc->setTerrainContact(true);
                for (auto& cp : contacts) {
                    cp.localA = cp.worldPos - body->pos();
                    cp.localB = cp.worldPos - origin;
                    cc->addContact(cp);
                }
                contactConstraints_.push_back(std::move(cc));
            }
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

    for (int iter = 0; iter < jointSolverExtraIterations_; ++iter) {
        for (auto& c : jointConstraints_) c->solveVelocity();
        for (auto  c : jointRefs_)        c->solveVelocity();
    }

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
