#include "rspch.hpp"
#include "physicsWorld.hpp"

// Vertical air resistance coefficient (y-axis only).
// linearDamping is used for horizontal ground friction.
// Terminal fall speed = gravity / kAirDamping (e.g. 9.8 / 0.15 ≈ 65 m/s).
// A smaller value raises terminal speed and lengthens the time constant
// (τ = 1/kAirDamping), so early fall closely tracks real free-fall g·t.
// NOTE: must match the client value to keep prediction and authority in sync.
static constexpr float kAirDamping = 0.15f;

PhysicsWorld::PhysicsWorld()
    : broadPhase_(std::make_unique<SAPBroadPhase>())
{}

void PhysicsWorld::registerBody(RigidBody* body,
                                std::function<void()> onRebuildBVH)
{
    entries_.push_back({ body, std::move(onRebuildBVH) });
    broadPhase_->add(body);
}

PhysicsWorld::TerrainHandle PhysicsWorld::registerTerrain(RigidBody* terrainBody,
                                    const TerrainHeightField* hf)
{
    return worldColliders_.add(std::make_unique<TerrainCollider>(terrainBody, hf));
}

void PhysicsWorld::unregisterTerrain(TerrainHandle handle)
{
    worldColliders_.remove(handle);
}

PhysicsWorld::ScatterHandle PhysicsWorld::registerScatter(std::unique_ptr<ScatterCollider> collider)
{
    return worldColliders_.add(std::move(collider));
}

void PhysicsWorld::unregisterScatter(ScatterHandle handle)
{
    worldColliders_.remove(handle);
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
    const int     n     = subStepCount_;
    const Seconds subDt = Seconds(dt.count() / n);

    for (auto& e : entries_) e.body->advanceState();

    for (int s = 0; s < n; ++s) {
        currentSubDt_ = subDt;
        integrate(subDt);
        generateContacts();
        solveConstraints(subDt);

        // Static obstacles (scatter props) get the final word on position: after
        // the dynamic solver so a monster squeezed against a tree ends up outside
        // it. resolveStaticPenetration writes pos directly, so rebuild the BVH of
        // any body it moved before the next sub-step's narrow phase.
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

            // Velocity motor: drive XZ velocity toward desiredVel.
            // Applied after damping so knockback bleeds off first,
            // then motor corrects toward desired each sub-step.
            if (b.motorEnabled()) {
                const mu::Vec3 vCurr    = b.linearVel();
                const mu::Vec3 vDesired = b.desiredVel();
                const mu::Vec3 vError   = mu::Vec3(
                    vDesired.x() - vCurr.x(), 0.f, vDesired.z() - vCurr.z());

                const float errLen = vError.len();
                if (errLen > 1e-4f) {
                    const float curSpd = mu::Vec3(vCurr.x(),    0.f, vCurr.z()).len();
                    const float dstSpd = mu::Vec3(vDesired.x(), 0.f, vDesired.z()).len();
                    const float maxA   = (dstSpd < curSpd)
                        ? b.motor().maxDeceleration
                        : b.motor().maxAcceleration;

                    // Proportional control clamped to maxA.
                    const float corrMag = std::min(errLen * b.motor().gain, maxA);
                    const mu::Vec3 dv   = (vError * (corrMag / errLen)) * dtf;

                    b.setLinearVel(mu::Vec3(
                        vCurr.x() + dv.x(), vCurr.y(), vCurr.z() + dv.z()));
                }
            }

            const auto linAcc = gravity_ + b.forceAccum() * b.invMass();
            b.setLinearVel(b.linearVel() + linAcc * dtf);

            b.updateInertiaWorld();
            const auto angAcc = b.torqueAccum() * b.invInertiaWorld();
            b.setOmega(b.omega() + angAcc * dtf);

            // Upright correction: restoring torque proportional to tilt from world Y-up.
            if (b.uprightStiffness() > 0.f) {
                const mu::Vec3 bodyUp   = b.orient().rotate(mu::Vec3(0.f, 1.f, 0.f));
                const mu::Vec3 torqDir  = mu::cross(bodyUp, mu::Vec3(0.f, 1.f, 0.f));
                b.setOmega(b.omega() + (torqDir * (b.uprightStiffness() * dtf)) * b.invInertiaWorld());
            }

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
    staticContacts_.clear();

    broadPhase_->update();
    const auto pairs = broadPhase_->queryPairs();

    for (const auto& [a, b] : pairs) {
        // Collision filter: skip pairs whose category/mask don't mutually match
        // (e.g. tactical troops pass through players / their own boss).
        if ((a->collisionCategory() & b->collisionMask()) == 0 ||
            (b->collisionCategory() & a->collisionMask()) == 0) continue;

        const BVH& bvhA = a->worldBVH();
        const BVH& bvhB = b->worldBVH();

        if (bvhA.empty() || bvhB.empty()) continue;

        const CollisionResult res = collides(bvhA, bvhB);
        if (!res.hit) continue;

        auto cc = std::make_unique<ContactConstraint>(a, b);

        // Contact normal: from narrow-phase geometry (B��A convention, fixed in collision.cpp).
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

    // --- Body vs static world colliders (terrain + scatter props) ---
    // For each Dynamic body, query every registered collider (each self-rejects
    // by footprint). Terrain emits support ContactConstraints; scatter emits
    // push-out StaticContacts resolved later by resolveStaticPenetration(). The
    // Dynamic-only filter encodes authority: on the server only monsters are
    // Dynamic (monster-vs-prop); players are Kinematic (client-authoritative).
    ContactSink sink;
    sink.constraints    = &contactConstraints_;
    sink.staticContacts = &staticContacts_;
    sink.gravity        = gravity_;
    sink.subDtSec       = currentSubDt_.count();
    for (auto& e : entries_) {
        RigidBody* body = e.body;
        if (body->motionType() != MotionType::Dynamic) continue;
        if (body->worldBVH().empty()) continue;
        constexpr float kPad = 4.f;   // body half-extent + look-ahead tolerance
        worldColliders_.forEach([&](const WorldCollider& wc) {
            if (wc.footprintReject(body->pos(), kPad)) return;
            wc.generateContacts(*body, sink);
        });
    }
}

void PhysicsWorld::solveConstraints(Seconds dt)
{
    auto allConstraints = [&](auto fn) {
        for (auto& c : contactConstraints_) fn(*c);
        for (auto& c : jointConstraints_)   fn(*c);
    };

    allConstraints([&](Constraint& c) { c.prepare(dt); });

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

    for (int iter = 0; iter < positionSolveIterations_; ++iter) {
        allConstraints([](Constraint& c) { c.solvePosition(); });
        for (auto& e : entries_) {
            e.body->applyPseudoVelocity(dt);
            e.body->clearPseudoVelocities();
        }
    }

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
