#ifndef __PhysicsTestObject_HPP
#define __PhysicsTestObject_HPP

#include "physicsWorld.hpp"
#include "collision.hpp"
#include "debugBVView.hpp"
#include <memory>
#include <vector>
#include <cmath>

// Build (or rebuild) the world-space single-OBB BVH for a box-shaped rigid body.
// This mirrors the ragdoll::rebuildBoneBodyBVH logic so test bodies participate
// in TerrainCollider contact generation the same way ragdoll bones do.
inline void rebuildBoxBodyBVH(RigidBody* body, mu::Vec3 he)
{
    OBB obb;
    obb.center      = body->pos();
    obb.halfExtents = he;
    obb.orient      = body->orient();

    // Enclosing AABB: project OBB half-extents onto each world axis.
    const mu::Mat4x4 rotMat(body->orient());
    const auto r0 = rotMat.row(0);
    const auto r1 = rotMat.row(1);
    const auto r2 = rotMat.row(2);
    const float wx = std::abs(r0.x()) * he.x() + std::abs(r1.x()) * he.y() + std::abs(r2.x()) * he.z();
    const float wy = std::abs(r0.y()) * he.x() + std::abs(r1.y()) * he.y() + std::abs(r2.y()) * he.z();
    const float wz = std::abs(r0.z()) * he.x() + std::abs(r1.z()) * he.y() + std::abs(r2.z()) * he.z();

    AABB bounds;
    bounds.center = obb.center;
    bounds.size   = mu::Vec3(wx * 2.f, wy * 2.f, wz * 2.f);

    BVH& bvh = body->worldBVH();
    if (bvh.nodes.empty()) bvh.nodes.resize(1);
    bvh.nodes[0].shape    = obb;
    bvh.nodes[0].bounds   = bounds;
    bvh.nodes[0].children.clear();
    bvh.nodes[0].boneIdx  = -1;
}

// A self-contained collection of RigidBodies and Constraints for isolated
// physics constraint testing. No rendering model is needed; bodies are
// visualized as OBB wireframes via DebugBVView.
//
// Usage:
//   1. Build bodies/joints with makePhysicsTestObject() below.
//   2. Call activate() to register with the physics world.
//   3. Call visualize() each frame to push OBBs to the debug view.
//   4. Call deactivate() before destroying to clean up physics world refs.
struct PhysicsTestObject {
    std::vector<std::unique_ptr<RigidBody>>  bodies;
    std::vector<mu::Vec3>                    halfExtents; // per-body box half-sizes (index matches bodies)
    std::vector<std::unique_ptr<Constraint>> joints;
    // Per-pair collision ignores: 1-hop joint pairs + 2-hop sibling pairs.
    // Populated by factory functions before activate().
    std::vector<std::pair<RigidBody*, RigidBody*>> ignoredPairs;

    // Register all bodies and joint refs.
    // Bodies receive a proper BVH rebuild callback so TerrainCollider and
    // the broad phase track them correctly on every step.
    // Group=1 / mask=0xFFFF: test bodies collide with terrain and all game objects.
    void activate(PhysicsWorld& pw) {
        for (int i = 0; i < static_cast<int>(bodies.size()); ++i) {
            RigidBody* b = bodies[i].get();
            const mu::Vec3 he = (i < static_cast<int>(halfExtents.size()))
                ? halfExtents[i] : mu::Vec3{ 0.15f, 0.15f, 0.15f };

            // Seed initial BVH before registration.
            rebuildBoxBodyBVH(b, he);

            pw.registerBody(b,
                [b, he]() { rebuildBoxBodyBVH(b, he); });
        }
        for (auto& j : joints)
            pw.addJointRef(j.get());
        for (const auto& [a, b] : ignoredPairs)
            pw.setIgnoreCollision(a, b, true);
    }

    // Remove all joint refs and bodies from the physics world.
    // Joints are removed first because they hold raw RigidBody pointers.
    void deactivate(PhysicsWorld& pw) {
        for (auto& j : joints) pw.removeJointRef(j.get());
        for (auto& b : bodies) pw.unregisterBody(b.get());
        for (const auto& [a, b] : ignoredPairs)
            pw.setIgnoreCollision(a, b, false);
    }

    // Push one OBB per body into the debug view with the given TTL.
    // Call each frame with a short TTL (e.g. 32ms) to produce a live display.
    // Kinematic anchors are tinted apart from the simulated bodies so the joint
    // structure stays readable in a still capture.
    void visualize(DebugBVView& view, Milliseconds ttl,
                   mu::Vec4 dynColor    = { 0.f, 1.f, 0.f, 1.f },
                   mu::Vec4 anchorColor = { 0.55f, 0.55f, 0.60f, 1.f }) const {
        for (int i = 0; i < static_cast<int>(bodies.size()); ++i) {
            const mu::Vec3 he = (i < static_cast<int>(halfExtents.size()))
                ? halfExtents[i] : mu::Vec3{ 0.15f, 0.15f, 0.15f };
            const bool dynamic = bodies[i]->motionType() == MotionType::Dynamic;
            view.push(OBB{ bodies[i]->pos(), he, bodies[i]->orient() }, ttl,
                      BVPipeline::BVModel::Box, dynamic ? dynColor : anchorColor);
        }
    }

    // Apply an instantaneous impulse at the center-of-mass of every Dynamic body.
    void applyImpulseAll(const mu::Vec3& imp) {
        for (auto& b : bodies)
            if (b->motionType() == MotionType::Dynamic)
                b->applyImpulse(imp, b->pos());
    }

    // Apply a random-direction impulse to every Dynamic body. The vertical
    // component is biased upward so a blast throws bodies outward instead of
    // driving them into the floor. Uses the shared gRandomEngine (pch.hpp).
    void applyRandomImpulse(float strength);

    // Zero linear and angular velocities of every Dynamic body (freeze).
    void freezeAll() {
        for (auto& b : bodies) {
            if (b->motionType() != MotionType::Dynamic) continue;
            b->setLinearVel({});
            b->setOmega({});
        }
    }
};

// ---------------------------------------------------------------------------
// Test structure factories (physicsTestObject.cpp)
//
// Each factory builds a PhysicsTestObject with a small set of rigid bodies
// connected by one joint type, centred around 'origin'.
// Pivot convention: anchorA = {0,0,0} places the pivot at body A's CoM.
//                   anchorB = {0, dist, 0} places the pivot 'dist' metres
//                   above body B's CoM, equalling body A's CoM when
//                   body B is spawned 'dist' metres below body A.
//
// These live in a .cpp on purpose: inlining ~600 lines of construction code
// into every including TU pushed onlineGame.obj past the COFF section limit
// (C1128), and nothing here is hot enough to want inlining.
//
// kind: 1=pendulum        2=doublePendulum   3=hingeDoor        4=coneTwistArm
//       5=coneTwistChain  6=humanoidRagdoll  7=upperBodyRagdoll 8=lowerBodyRagdoll
// Out-of-range kinds yield an empty object (activate() then does nothing).
// ---------------------------------------------------------------------------
PhysicsTestObject makePhysicsTestObject(int kind, mu::Vec3 origin);

// Ragdoll-style structures (kind >= 6) have branching or deep joint chains that
// cannot converge within the default 4 velocity passes. Extra joint-only
// iterations are cheap because contacts are unaffected.
inline bool physicsTestObjectNeedsExtraIterations(int kind) { return kind >= 6; }

#endif // __PhysicsTestObject_HPP
