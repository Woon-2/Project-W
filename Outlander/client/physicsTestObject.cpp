#include "pch.hpp"
#include "physicsTestObject.hpp"
#include "jointConstraint.hpp"

#include <set>
#include <unordered_map>

// Apply a random-direction impulse to every Dynamic body. The vertical component
// is biased upward so a blast throws bodies outward instead of driving them into
// the floor. Uses the shared gRandomEngine (pch.hpp) -- never a local mt19937.
void PhysicsTestObject::applyRandomImpulse(float strength) {
    for (auto& b : bodies) {
        if (b->motionType() != MotionType::Dynamic) continue;
        const mu::Vec3 rawDir{
            rand(-1.f, 1.f),
            std::abs(rand(-1.f, 1.f)) * 0.5f + 0.3f,
            rand(-1.f, 1.f)
        };
        b->applyImpulse(mu::Vec3(mu::NVec3(rawDir)) * strength, b->pos());
    }
}

// ---------------------------------------------------------------------------
// Physics test object factories
//
// Each factory builds a PhysicsTestObject with a small set of rigid bodies
// connected by one joint type, centred around 'origin'.
// Pivot convention: anchorA = {0,0,0} places the pivot at body A's CoM.
//                   anchorB = {0, dist, 0} places the pivot 'dist' metres
//                   above body B's CoM, equalling body A's CoM when
//                   body B is spawned 'dist' metres below body A.
// ---------------------------------------------------------------------------

static std::unique_ptr<RigidBody> makeAnchorBody(mu::Vec3 pos) {
    auto b = std::make_unique<RigidBody>(MotionType::Kinematic);
    b->setPos(pos);
    b->snapToCurrent();
    return b;
}

static std::unique_ptr<RigidBody> makeDynBody(mu::Vec3 pos, float mass,
                                               float linDamp = 2.f, float angDamp = 4.f) {
    auto b = std::make_unique<RigidBody>(MotionType::Dynamic);
    b->setMass(mass);
    b->setLinearDamping(linDamp);
    b->setAngularDamping(angDamp);
    b->setPos(pos);
    b->snapToCurrent();
    return b;
}

// 1 - Single pendulum: anchor + 1 bob connected by a BallSocket.
//     Arm length 1.5 m (anchor CoM to bob CoM).
static PhysicsTestObject makePendulum(mu::Vec3 origin) {
    const mu::Vec3 anchorPos = origin + mu::Vec3{ 0.f, 5.f, 0.f };
    const mu::Vec3 bobPos    = anchorPos + mu::Vec3{ 0.f, -1.5f, 0.f };
    const mu::Vec3 anchorHe{ 0.15f, 0.15f, 0.15f };
    const mu::Vec3 bobHe   { 0.20f, 0.20f, 0.20f };

    auto anchor = makeAnchorBody(anchorPos);
    auto bob    = makeDynBody(bobPos, 1.f);

    auto joint = std::make_unique<BallSocketJoint>(
        anchor.get(), bob.get(),
        mu::Vec3{ 0.f, 0.f, 0.f },   // anchorA: pivot at anchor CoM
        mu::Vec3{ 0.f, 1.5f, 0.f }   // anchorB: pivot 1.5 m above bob CoM = anchorPos
    );

    PhysicsTestObject obj;
    obj.bodies.push_back(std::move(anchor));
    obj.bodies.push_back(std::move(bob));
    obj.halfExtents = { anchorHe, bobHe };
    obj.joints.push_back(std::move(joint));
    return obj;
}

// 2 - Double pendulum: anchor + 2 links, each connected by a BallSocket.
//     Tests chain solver convergence with chaotic motion under gravity.
static PhysicsTestObject makeDoublePendulum(mu::Vec3 origin) {
    const mu::Vec3 anchorPos = origin + mu::Vec3{ 0.f, 5.f, 0.f };
    const mu::Vec3 link1Pos  = anchorPos + mu::Vec3{ 0.f, -1.2f, 0.f };
    const mu::Vec3 link2Pos  = link1Pos  + mu::Vec3{ 0.f, -1.2f, 0.f };
    const mu::Vec3 anchorHe{ 0.12f, 0.12f, 0.12f };
    const mu::Vec3 linkHe  { 0.15f, 0.15f, 0.15f };

    auto anchor = makeAnchorBody(anchorPos);
    auto link1  = makeDynBody(link1Pos, 1.f);
    auto link2  = makeDynBody(link2Pos, 1.f);

    // Joint 0: anchor -> link1; pivot at anchor CoM.
    auto j0 = std::make_unique<BallSocketJoint>(
        anchor.get(), link1.get(),
        mu::Vec3{ 0.f, 0.f, 0.f },
        mu::Vec3{ 0.f, 1.2f, 0.f }
    );
    // Joint 1: link1 -> link2; pivot at link1 CoM.
    auto j1 = std::make_unique<BallSocketJoint>(
        link1.get(), link2.get(),
        mu::Vec3{ 0.f, 0.f, 0.f },
        mu::Vec3{ 0.f, 1.2f, 0.f }
    );

    PhysicsTestObject obj;
    obj.bodies.push_back(std::move(anchor));
    obj.bodies.push_back(std::move(link1));
    obj.bodies.push_back(std::move(link2));
    obj.halfExtents = { anchorHe, linkHe, linkHe };
    obj.joints.push_back(std::move(j0));
    obj.joints.push_back(std::move(j1));
    return obj;
}

// 3 - Hinge door: static wall block + swinging door panel connected by a HingeJoint.
//     Hinge axis: world Y (vertical). Angular limits: +/-120 deg.
static PhysicsTestObject makeHingeDoor(mu::Vec3 origin) {
    const mu::Vec3 wallPos = origin + mu::Vec3{ 0.f, 2.f, 0.f };
    const mu::Vec3 wallHe{ 0.15f, 0.50f, 0.15f };
    const mu::Vec3 doorHe{ 0.80f, 0.50f, 0.05f };

    // Door is placed so its left face aligns with wall's right face at the hinge pivot.
    // World pivot = wallPos + {wallHe.x, 0, 0} = wallPos + {0.15, 0, 0}
    // Door CoM    = pivot   + {doorHe.x, 0, 0} = wallPos + {0.95, 0, 0}
    const mu::Vec3 doorPos = wallPos + mu::Vec3{ wallHe.x() + doorHe.x(), 0.f, 0.f };

    auto wall = makeAnchorBody(wallPos);
    auto door = makeDynBody(doorPos, 5.f, 2.f, 4.f);

    // anchorA: right face of wall (+X)
    // anchorB: left face of door  (-X), so world pivot = doorPos + {-0.80, 0, 0} = wallPos + {0.15, 0, 0}
    auto joint = std::make_unique<HingeJoint>(
        wall.get(), door.get(),
        mu::Vec3{  wallHe.x(), 0.f, 0.f },
        mu::Vec3{ -doorHe.x(), 0.f, 0.f },
        mu::Vec3{  0.f, 1.f, 0.f },          // hinge axis: world Y in wall local space
        -mu::pi * 2.f / 3.f,                  // -120 deg
         mu::pi * 2.f / 3.f                   // +120 deg
    );

    PhysicsTestObject obj;
    obj.bodies.push_back(std::move(wall));
    obj.bodies.push_back(std::move(door));
    obj.halfExtents = { wallHe, doorHe };
    obj.joints.push_back(std::move(joint));
    return obj;
}

// 4 - Cone-twist arm: static shoulder + dangling arm connected by a ConeTwistJoint.
//     Tests both swing (cone 45 deg) and twist (30 deg) limits simultaneously.
static PhysicsTestObject makeConeTwistArm(mu::Vec3 origin) {
    const mu::Vec3 shoulderPos = origin + mu::Vec3{ 0.f, 4.f, 0.f };
    const mu::Vec3 armPos      = shoulderPos + mu::Vec3{ 0.f, -1.5f, 0.f };
    const mu::Vec3 shoulderHe{ 0.20f, 0.20f, 0.20f };
    const mu::Vec3 armHe     { 0.10f, 0.40f, 0.10f };

    auto shoulder = makeAnchorBody(shoulderPos);
    auto arm      = makeDynBody(armPos, 1.f, 1.5f, 5.f);

    // refOrients: both bodies at identity orientation at construction.
    const mu::NQuat refA = shoulder->orient();
    const mu::NQuat refB = arm->orient();

    auto joint = std::make_unique<ConeTwistJoint>(
        shoulder.get(), arm.get(),
        mu::Vec3{ 0.f, 0.f, 0.f },   // anchorA: pivot at shoulder CoM
        mu::Vec3{ 0.f, 1.5f, 0.f },  // anchorB: pivot 1.5 m above arm CoM = shoulderPos
        refA, refB,
        mu::pi / 4.f,    // cone half-angle: 45 deg
        mu::pi / 6.f     // twist limit:     30 deg
    );

    PhysicsTestObject obj;
    obj.bodies.push_back(std::move(shoulder));
    obj.bodies.push_back(std::move(arm));
    obj.halfExtents = { shoulderHe, armHe };
    obj.joints.push_back(std::move(joint));
    return obj;
}

// 5 - Cone-twist chain: anchor + 5 links, each connected by a ConeTwistJoint.
//     Tests simultaneous cone+twist limit enforcement across multiple chained joints.
//     Cone: 30 deg, Twist: 22.5 deg per link.
static PhysicsTestObject makeConeTwistChain(mu::Vec3 origin) {
    constexpr int    kLinks     = 5;
    constexpr float  kSpacing   = 0.30f;  // centre-to-centre gap between consecutive links
    const mu::Vec3 anchorHe { 0.06f, 0.06f, 0.06f };
    const mu::Vec3 linkHe   { 0.05f, 0.10f, 0.05f };

    const mu::Vec3 anchorPos = origin + mu::Vec3{ 0.f, 5.f, 0.f };

    PhysicsTestObject obj;
    obj.bodies.push_back(makeAnchorBody(anchorPos));
    obj.halfExtents.push_back(anchorHe);

    for (int i = 0; i < kLinks; ++i) {
		const float mass = (kLinks - i) * 2.f;
        const mu::Vec3 linkPos = anchorPos + mu::Vec3{ 0.f, -kSpacing * (i + 1), 0.f };
        obj.bodies.push_back(makeDynBody(linkPos, (kLinks - i) * 2.f, 1.5f, 4.f));
		auto& bd = obj.bodies.back();
		bd->setInertia(computeBoxInertia(mass, linkHe));
        obj.halfExtents.push_back(linkHe);
    }

    // Connect consecutive bodies with ConeTwist joints.
    // anchorA = {0,0,0}: pivot at the upper body's CoM.
    // anchorB = {0, kSpacing, 0}: pivot kSpacing above lower body's CoM = upper body CoM.
    for (int i = 0; i < kLinks; ++i) {
        RigidBody* upper = obj.bodies[i].get();
        RigidBody* lower = obj.bodies[i + 1].get();
        obj.joints.push_back(std::make_unique<ConeTwistJoint>(
            upper, lower,
            mu::Vec3{ 0.f, 0.f, 0.f },
            mu::Vec3{ 0.f, kSpacing, 0.f },
            upper->orient(), lower->orient(),
            mu::pi / 3.f,     // cone half-angle: 60 deg
            mu::pi / 4.f      // twist limit:     45 deg
        ));
		obj.joints.back()->resetAnchors();
    }

    return obj;
}

// 6 - Humanoid ragdoll: 12 Dynamic bodies (A-pose) connected by 11 joints.
//     Layout mirrors getHumanoidRagdollDef() but requires no skeleton or model.
//     All bodies start at identity orientation (arms hang downward at sides).
//     Body positions, half-extents, masses, and joint limits match the
//     production ragdoll definition so simulation behaviour is representative.
static PhysicsTestObject makeHumanoidRagdoll(mu::Vec3 origin) {
    constexpr int kHips          = 0;
    constexpr int kSpine         = 1;
    constexpr int kChest         = 2;
    constexpr int kHead          = 3;
    constexpr int kLeftUpperArm  = 4;
    constexpr int kLeftLowerArm  = 5;
    constexpr int kRightUpperArm = 6;
    constexpr int kRightLowerArm = 7;
    constexpr int kLeftUpperLeg  = 8;
    constexpr int kLeftLowerLeg  = 9;
    constexpr int kRightUpperLeg = 10;
    constexpr int kRightLowerLeg = 11;

    constexpr float kLin = 0.1f;
    constexpr float kAng = 0.2f;

    const mu::Vec3 heHips     { 0.12f, 0.08f, 0.10f };
    const mu::Vec3 heSpine    { 0.10f, 0.10f, 0.09f };
    const mu::Vec3 heChest    { 0.11f, 0.10f, 0.09f };
    const mu::Vec3 heHead     { 0.10f, 0.10f, 0.10f };
    const mu::Vec3 heUpperArm { 0.06f, 0.14f, 0.06f };
    const mu::Vec3 heLowerArm { 0.05f, 0.12f, 0.05f };
    const mu::Vec3 heUpperLeg { 0.08f, 0.20f, 0.08f };
    const mu::Vec3 heLowerLeg { 0.06f, 0.18f, 0.06f };

    const float masses[12] = { 12.f, 8.f, 8.f, 5.f,
                                3.f, 2.f, 3.f, 2.f,
                                8.f, 5.f, 8.f, 5.f };
    const mu::Vec3 hes[12] = { heHips, heSpine, heChest, heHead,
                                heUpperArm, heLowerArm, heUpperArm, heLowerArm,
                                heUpperLeg, heLowerLeg, heUpperLeg, heLowerLeg };

    // Body CoM positions relative to origin (feet of the humanoid).
    // Shoulder pivots are at ±0.22 on X at chest height (1.38).
    // Hip sockets are at ±0.11 on X, 0.10 below hips CoM.
    const mu::Vec3 positions[12] = {
        origin + mu::Vec3{  0.f,    1.00f, 0.f },  // Hips
        origin + mu::Vec3{  0.f,    1.18f, 0.f },  // Spine
        origin + mu::Vec3{  0.f,    1.38f, 0.f },  // Chest
        origin + mu::Vec3{  0.f,    1.58f, 0.f },  // Head
        origin + mu::Vec3{ -0.22f,  1.24f, 0.f },  // LeftUpperArm
        origin + mu::Vec3{ -0.22f,  0.98f, 0.f },  // LeftLowerArm
        origin + mu::Vec3{  0.22f,  1.24f, 0.f },  // RightUpperArm
        origin + mu::Vec3{  0.22f,  0.98f, 0.f },  // RightLowerArm
        origin + mu::Vec3{ -0.11f,  0.70f, 0.f },  // LeftUpperLeg
        origin + mu::Vec3{ -0.11f,  0.32f, 0.f },  // LeftLowerLeg
        origin + mu::Vec3{  0.11f,  0.70f, 0.f },  // RightUpperLeg
        origin + mu::Vec3{  0.11f,  0.32f, 0.f },  // RightLowerLeg
    };

    PhysicsTestObject obj;
    for (int i = 0; i < 12; ++i) {
        obj.bodies.push_back(makeDynBody(positions[i], masses[i], kLin, kAng));
        obj.bodies.back()->setInertia(computeBoxInertia(masses[i], hes[i]));
        obj.halfExtents.push_back(hes[i]);
    }

    RigidBody* bHips = obj.bodies[kHips].get();
    RigidBody* bSpine = obj.bodies[kSpine].get();
    RigidBody* bChest = obj.bodies[kChest].get();
    RigidBody* bHead  = obj.bodies[kHead].get();
    RigidBody* bLUA   = obj.bodies[kLeftUpperArm].get();
    RigidBody* bLLA   = obj.bodies[kLeftLowerArm].get();
    RigidBody* bRUA   = obj.bodies[kRightUpperArm].get();
    RigidBody* bRLA   = obj.bodies[kRightLowerArm].get();
    RigidBody* bLUL   = obj.bodies[kLeftUpperLeg].get();
    RigidBody* bLLL   = obj.bodies[kLeftLowerLeg].get();
    RigidBody* bRUL   = obj.bodies[kRightUpperLeg].get();
    RigidBody* bRLL   = obj.bodies[kRightLowerLeg].get();
    const mu::NQuat idQ{};

    // Spine chain — ConeTwist, Y-up twist axis.
    obj.joints.push_back(std::make_unique<ConeTwistJoint>(
        bHips, bSpine,
        mu::Vec3{ 0.f,  0.08f, 0.f }, mu::Vec3{ 0.f, -0.10f, 0.f },
        idQ, idQ, mu::Vec3{ 0.f, 1.f, 0.f },
        mu::pi * 0.2f, mu::pi * 0.15f));
    obj.joints.push_back(std::make_unique<ConeTwistJoint>(
        bSpine, bChest,
        mu::Vec3{ 0.f,  0.10f, 0.f }, mu::Vec3{ 0.f, -0.10f, 0.f },
        idQ, idQ, mu::Vec3{ 0.f, 1.f, 0.f },
        mu::pi * 0.2f, mu::pi * 0.15f));
    obj.joints.push_back(std::make_unique<ConeTwistJoint>(
        bChest, bHead,
        mu::Vec3{ 0.f,  0.10f, 0.f }, mu::Vec3{ 0.f, -0.10f, 0.f },
        idQ, idQ, mu::Vec3{ 0.f, 1.f, 0.f },
        mu::pi * 0.3f, mu::pi * 0.2f));

    // Arm chains — ConeTwist at shoulder, Hinge at elbow.
    // A-pose: arms hang down, so the bone direction from shoulder is (0,-1,0).
    obj.joints.push_back(std::make_unique<ConeTwistJoint>(
        bChest, bLUA,
        mu::Vec3{ -0.22f, 0.f, 0.f }, mu::Vec3{ 0.f, 0.14f, 0.f },
        idQ, idQ, mu::Vec3{ 0.f, -1.f, 0.f },
        mu::pi * 0.6f, mu::pi * 0.5f));
    obj.joints.push_back(std::make_unique<HingeJoint>(
        bLUA, bLLA,
        mu::Vec3{ 0.f, -0.14f, 0.f }, mu::Vec3{ 0.f, 0.12f, 0.f },
        mu::Vec3{ 0.f, 0.f, 1.f }, 0.f, mu::pi * 0.9f));
    obj.joints.push_back(std::make_unique<ConeTwistJoint>(
        bChest, bRUA,
        mu::Vec3{  0.22f, 0.f, 0.f }, mu::Vec3{ 0.f, 0.14f, 0.f },
        idQ, idQ, mu::Vec3{ 0.f, -1.f, 0.f },
        mu::pi * 0.6f, mu::pi * 0.5f));
    obj.joints.push_back(std::make_unique<HingeJoint>(
        bRUA, bRLA,
        mu::Vec3{ 0.f, -0.14f, 0.f }, mu::Vec3{ 0.f, 0.12f, 0.f },
        mu::Vec3{ 0.f, 0.f, 1.f }, 0.f, mu::pi * 0.9f));

    // Leg chains — ConeTwist at hip socket, Hinge at knee.
    // A-pose: legs hang straight down, so the bone direction from hip socket is (0,-1,0).
    obj.joints.push_back(std::make_unique<ConeTwistJoint>(
        bHips, bLUL,
        mu::Vec3{ -0.11f, -0.10f, 0.f }, mu::Vec3{ 0.f, 0.20f, 0.f },
        idQ, idQ, mu::Vec3{ 0.f, -1.f, 0.f },
        mu::pi * 0.5f, mu::pi * 0.25f));
    obj.joints.push_back(std::make_unique<HingeJoint>(
        bLUL, bLLL,
        mu::Vec3{ 0.f, -0.20f, 0.f }, mu::Vec3{ 0.f, 0.18f, 0.f },
        mu::Vec3{ 1.f, 0.f, 0.f }, 0.f, mu::pi * 0.9f));
    obj.joints.push_back(std::make_unique<ConeTwistJoint>(
        bHips, bRUL,
        mu::Vec3{  0.11f, -0.10f, 0.f }, mu::Vec3{ 0.f, 0.20f, 0.f },
        idQ, idQ, mu::Vec3{ 0.f, -1.f, 0.f },
        mu::pi * 0.5f, mu::pi * 0.25f));
    obj.joints.push_back(std::make_unique<HingeJoint>(
        bRUL, bRLL,
        mu::Vec3{ 0.f, -0.20f, 0.f }, mu::Vec3{ 0.f, 0.18f, 0.f },
        mu::Vec3{ 1.f, 0.f, 0.f }, 0.f, mu::pi * 0.9f));

    for (auto& j : obj.joints)
        j->resetAnchors();

    // Collision ignore pairs: 1-hop (directly connected) + 2-hop (common neighbour).
    // Mirrors Ragdoll::activate() to prevent intra-body contact instability.
    {
        const std::pair<RigidBody*, RigidBody*> jointPairs[] = {
            {bHips, bSpine},
            {bSpine, bChest},
            {bChest, bHead},
            {bChest, bLUA},
            {bLUA,   bLLA},
            {bChest, bRUA},
            {bRUA,   bRLA},
            {bHips,  bLUL},
            {bLUL,   bLLL},
            {bHips,  bRUL},
            {bRUL,   bRLL},
        };

        auto normPair = [](RigidBody* a, RigidBody* b) {
            return a < b ? std::make_pair(a, b) : std::make_pair(b, a);
        };
        std::set<std::pair<RigidBody*, RigidBody*>> pending;
        for (const auto& [a, b] : jointPairs)
            pending.insert(normPair(a, b));

        std::unordered_map<RigidBody*, std::vector<RigidBody*>> adj;
        for (const auto& [a, b] : jointPairs) {
            adj[a].push_back(b);
            adj[b].push_back(a);
        }
        for (auto& [mid, nb] : adj) {
            const int n = static_cast<int>(nb.size());
            for (int i = 0; i < n; ++i)
                for (int j = i + 1; j < n; ++j)
                    pending.insert(normPair(nb[i], nb[j]));
        }

        for (const auto& p : pending)
            obj.ignoredPairs.push_back(p);
    }

    return obj;
}

// 7 - Upper body ragdoll: Kinematic Hips anchor + Dynamic spine/chest/head/arms (8 bodies, 7 joints).
//     Isolates upper-body joint behaviour from leg interactions.
static PhysicsTestObject makeUpperBodyRagdoll(mu::Vec3 origin) {
    constexpr float kLin = 0.1f, kAng = 0.2f;
    const mu::Vec3 heHips { 0.12f, 0.08f, 0.10f };
    const mu::Vec3 heSpine{ 0.10f, 0.10f, 0.09f };
    const mu::Vec3 heChest{ 0.11f, 0.10f, 0.09f };
    const mu::Vec3 heHead { 0.10f, 0.10f, 0.10f };
    const mu::Vec3 heUA   { 0.06f, 0.14f, 0.06f };
    const mu::Vec3 heLa   { 0.05f, 0.12f, 0.05f };

    PhysicsTestObject obj;

    // index 0: Kinematic Hips (anchor)
    obj.bodies.push_back(makeAnchorBody(origin + mu::Vec3{ 0.f, 1.00f, 0.f }));
    obj.halfExtents.push_back(heHips);

    // indices 1-7: Dynamic upper-body parts
    const struct { mu::Vec3 off; float mass; mu::Vec3 he; } dyn[] = {
        { { 0.f,    1.18f, 0.f }, 8.f, heSpine },
        { { 0.f,    1.38f, 0.f }, 8.f, heChest },
        { { 0.f,    1.58f, 0.f }, 5.f, heHead  },
        { {-0.22f,  1.24f, 0.f }, 3.f, heUA    },
        { {-0.22f,  0.98f, 0.f }, 2.f, heLa    },
        { { 0.22f,  1.24f, 0.f }, 3.f, heUA    },
        { { 0.22f,  0.98f, 0.f }, 2.f, heLa    },
    };
    for (const auto& d : dyn) {
        obj.bodies.push_back(makeDynBody(origin + d.off, d.mass, kLin, kAng));
        obj.bodies.back()->setInertia(computeBoxInertia(d.mass, d.he));
        obj.halfExtents.push_back(d.he);
    }

    RigidBody* bHips  = obj.bodies[0].get();
    RigidBody* bSpine = obj.bodies[1].get();
    RigidBody* bChest = obj.bodies[2].get();
    RigidBody* bHead  = obj.bodies[3].get();
    RigidBody* bLUA   = obj.bodies[4].get();
    RigidBody* bLLA   = obj.bodies[5].get();
    RigidBody* bRUA   = obj.bodies[6].get();
    RigidBody* bRLA   = obj.bodies[7].get();
    const mu::NQuat idQ{};

    // Spine chain
    obj.joints.push_back(std::make_unique<ConeTwistJoint>(
        bHips, bSpine,
        mu::Vec3{ 0.f,  0.08f, 0.f }, mu::Vec3{ 0.f, -0.10f, 0.f },
        idQ, idQ, mu::Vec3{ 0.f, 1.f, 0.f },
        mu::pi * 0.2f, mu::pi * 0.15f));
    obj.joints.push_back(std::make_unique<ConeTwistJoint>(
        bSpine, bChest,
        mu::Vec3{ 0.f,  0.10f, 0.f }, mu::Vec3{ 0.f, -0.10f, 0.f },
        idQ, idQ, mu::Vec3{ 0.f, 1.f, 0.f },
        mu::pi * 0.2f, mu::pi * 0.15f));
    obj.joints.push_back(std::make_unique<ConeTwistJoint>(
        bChest, bHead,
        mu::Vec3{ 0.f,  0.10f, 0.f }, mu::Vec3{ 0.f, -0.10f, 0.f },
        idQ, idQ, mu::Vec3{ 0.f, 1.f, 0.f },
        mu::pi * 0.3f, mu::pi * 0.2f));

    // Arms: A-pose twist axis = (0,-1,0) (arms hang down).
    // BFS depth ordering: all depth-3 joints (Chest→*UA) before depth-4
    // joints (*UA→*LA) so each solver iteration propagates corrections
    // root-to-leaf (shock propagation).
    obj.joints.push_back(std::make_unique<ConeTwistJoint>(
        bChest, bLUA,
        mu::Vec3{ -0.22f, 0.f, 0.f }, mu::Vec3{ 0.f, 0.14f, 0.f },
        idQ, idQ, mu::Vec3{ 0.f, -1.f, 0.f },
        mu::pi * 0.6f, mu::pi * 0.5f));
    obj.joints.push_back(std::make_unique<ConeTwistJoint>(
        bChest, bRUA,
        mu::Vec3{  0.22f, 0.f, 0.f }, mu::Vec3{ 0.f, 0.14f, 0.f },
        idQ, idQ, mu::Vec3{ 0.f, -1.f, 0.f },
        mu::pi * 0.6f, mu::pi * 0.5f));
    obj.joints.push_back(std::make_unique<HingeJoint>(
        bLUA, bLLA,
        mu::Vec3{ 0.f, -0.14f, 0.f }, mu::Vec3{ 0.f, 0.12f, 0.f },
        mu::Vec3{ 0.f, 0.f, 1.f }, 0.f, mu::pi * 0.9f));
    obj.joints.push_back(std::make_unique<HingeJoint>(
        bRUA, bRLA,
        mu::Vec3{ 0.f, -0.14f, 0.f }, mu::Vec3{ 0.f, 0.12f, 0.f },
        mu::Vec3{ 0.f, 0.f, 1.f }, 0.f, mu::pi * 0.9f));

    for (auto& j : obj.joints)
        j->resetAnchors();

    {
        const std::pair<RigidBody*, RigidBody*> jointPairs[] = {
            {bHips, bSpine}, {bSpine, bChest}, {bChest, bHead},
            {bChest, bLUA}, {bLUA, bLLA},
            {bChest, bRUA}, {bRUA, bRLA},
        };
        auto normPair = [](RigidBody* a, RigidBody* b) {
            return a < b ? std::make_pair(a, b) : std::make_pair(b, a);
        };
        std::set<std::pair<RigidBody*, RigidBody*>> pending;
        for (const auto& [a, b] : jointPairs)
            pending.insert(normPair(a, b));
        std::unordered_map<RigidBody*, std::vector<RigidBody*>> adj;
        for (const auto& [a, b] : jointPairs) {
            adj[a].push_back(b);
            adj[b].push_back(a);
        }
        for (auto& [mid, nb] : adj) {
            const int n = static_cast<int>(nb.size());
            for (int i = 0; i < n; ++i)
                for (int j = i + 1; j < n; ++j)
                    pending.insert(normPair(nb[i], nb[j]));
        }
        for (const auto& p : pending)
            obj.ignoredPairs.push_back(p);
    }
    return obj;
}

// 8 - Lower body ragdoll: Kinematic Hips anchor + Dynamic legs (5 bodies, 4 joints).
//     Isolates hip-socket and knee joint behaviour from upper-body interactions.
static PhysicsTestObject makeLowerBodyRagdoll(mu::Vec3 origin) {
    constexpr float kLin = 0.1f, kAng = 0.2f;
    const mu::Vec3 heHips{ 0.12f, 0.08f, 0.10f };
    const mu::Vec3 heUL  { 0.08f, 0.20f, 0.08f };
    const mu::Vec3 heLL  { 0.06f, 0.18f, 0.06f };

    PhysicsTestObject obj;

    // index 0: Kinematic Hips (anchor)
    obj.bodies.push_back(makeAnchorBody(origin + mu::Vec3{ 0.f, 1.00f, 0.f }));
    obj.halfExtents.push_back(heHips);

    // indices 1-4: Dynamic leg parts
    const struct { mu::Vec3 off; float mass; mu::Vec3 he; } dyn[] = {
        { {-0.11f, 0.70f, 0.f }, 8.f, heUL },
        { {-0.11f, 0.32f, 0.f }, 5.f, heLL },
        { { 0.11f, 0.70f, 0.f }, 8.f, heUL },
        { { 0.11f, 0.32f, 0.f }, 5.f, heLL },
    };
    for (const auto& d : dyn) {
        obj.bodies.push_back(makeDynBody(origin + d.off, d.mass, kLin, kAng));
        obj.bodies.back()->setInertia(computeBoxInertia(d.mass, d.he));
        obj.halfExtents.push_back(d.he);
    }

    RigidBody* bHips = obj.bodies[0].get();
    RigidBody* bLUL  = obj.bodies[1].get();
    RigidBody* bLLL  = obj.bodies[2].get();
    RigidBody* bRUL  = obj.bodies[3].get();
    RigidBody* bRLL  = obj.bodies[4].get();
    const mu::NQuat idQ{};

    // Hip sockets: A-pose legs hang down → twist axis (0,-1,0).
    // BFS depth ordering: both depth-1 joints (Hips→*UL) before depth-2
    // joints (*UL→*LL).
    obj.joints.push_back(std::make_unique<ConeTwistJoint>(
        bHips, bLUL,
        mu::Vec3{ -0.11f, -0.10f, 0.f }, mu::Vec3{ 0.f, 0.20f, 0.f },
        idQ, idQ, mu::Vec3{ 0.f, -1.f, 0.f },
        mu::pi * 0.5f, mu::pi * 0.25f));
    obj.joints.push_back(std::make_unique<ConeTwistJoint>(
        bHips, bRUL,
        mu::Vec3{  0.11f, -0.10f, 0.f }, mu::Vec3{ 0.f, 0.20f, 0.f },
        idQ, idQ, mu::Vec3{ 0.f, -1.f, 0.f },
        mu::pi * 0.5f, mu::pi * 0.25f));
    obj.joints.push_back(std::make_unique<HingeJoint>(
        bLUL, bLLL,
        mu::Vec3{ 0.f, -0.20f, 0.f }, mu::Vec3{ 0.f, 0.18f, 0.f },
        mu::Vec3{ 1.f, 0.f, 0.f }, 0.f, mu::pi * 0.9f));
    obj.joints.push_back(std::make_unique<HingeJoint>(
        bRUL, bRLL,
        mu::Vec3{ 0.f, -0.20f, 0.f }, mu::Vec3{ 0.f, 0.18f, 0.f },
        mu::Vec3{ 1.f, 0.f, 0.f }, 0.f, mu::pi * 0.9f));

    for (auto& j : obj.joints)
        j->resetAnchors();

    {
        const std::pair<RigidBody*, RigidBody*> jointPairs[] = {
            {bHips, bLUL}, {bLUL, bLLL},
            {bHips, bRUL}, {bRUL, bRLL},
        };
        auto normPair = [](RigidBody* a, RigidBody* b) {
            return a < b ? std::make_pair(a, b) : std::make_pair(b, a);
        };
        std::set<std::pair<RigidBody*, RigidBody*>> pending;
        for (const auto& [a, b] : jointPairs)
            pending.insert(normPair(a, b));
        std::unordered_map<RigidBody*, std::vector<RigidBody*>> adj;
        for (const auto& [a, b] : jointPairs) {
            adj[a].push_back(b);
            adj[b].push_back(a);
        }
        for (auto& [mid, nb] : adj) {
            const int n = static_cast<int>(nb.size());
            for (int i = 0; i < n; ++i)
                for (int j = i + 1; j < n; ++j)
                    pending.insert(normPair(nb[i], nb[j]));
        }
        for (const auto& p : pending)
            obj.ignoredPairs.push_back(p);
    }
    return obj;
}

PhysicsTestObject makePhysicsTestObject(int kind, mu::Vec3 origin) {
    switch (kind) {
    case 1: return makePendulum(origin);
    case 2: return makeDoublePendulum(origin);
    case 3: return makeHingeDoor(origin);
    case 4: return makeConeTwistArm(origin);
    case 5: return makeConeTwistChain(origin);
    case 6: return makeHumanoidRagdoll(origin);
    case 7: return makeUpperBodyRagdoll(origin);
    case 8: return makeLowerBodyRagdoll(origin);
    default: return PhysicsTestObject{};
    }
}
