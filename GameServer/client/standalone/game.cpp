#include "pch.hpp"
#include "game.hpp"

#include "../skill/skillCompiler.hpp"
#include "../jointConstraint.hpp"
#include "../errorHandling.hpp"
#include "../binaryImport.hpp"
#include "../log.hpp"
#include "../timer.hpp"
#include "../particleImporter.hpp"
#include "../ui/widgets/Label.hpp"
#include "../ui/widgets/ProgressBar.hpp"

extern RECT gClientRect;

namespace StandAlone {

// ---------------------------------------------------------------------------
// Player movement parameters — tweak these to adjust game feel.
//
// kPlayerLinearDamping acts as ground friction: each physics step (60 Hz)
// velocity is multiplied by (1 - damping/60).
//
// For the player to sustain kPlayerMaxSpeed under constant input,
// kPlayerAccelRate must equal kPlayerMaxSpeed * kPlayerLinearDamping.
// (At equilibrium: added_vel_per_step == removed_vel_per_step.)
//
// Stop time from max speed (no input):
//   kPlayerLinearDamping = 10  → ~0.42 s
//   kPlayerLinearDamping = 12  → ~0.33 s  ← default
//   kPlayerLinearDamping = 20  → ~0.20 s  (very snappy)
// ---------------------------------------------------------------------------
static constexpr float kPlayerMaxSpeed      = 10.f;   // m/s
static constexpr float kPlayerLinearDamping = 12.f;   // adjust stop time
static constexpr float kPlayerAccelRate     = kPlayerMaxSpeed * kPlayerLinearDamping;

static constexpr int     kRenderSkipLagFrames = 4;
static constexpr int     kMaxPhysicsStepsPerFrame = 3;
static constexpr Seconds kMaxPhysicsDeltaTime{ 1.f / 60.f * kMaxPhysicsStepsPerFrame };
static constexpr int     kMaxPhysicsScaleK    = 4;   // physicUpdateInterval 최대 배율
static constexpr int     kLagScaleUpFrames    = 2;   // 연속 렉 N프레임 → 배율 1 증가
static constexpr int     kLagScaleDownFrames  = 100; // 연속 정상 N프레임 → 배율 1 감소
static constexpr float   kArrowRainRadius     = 4.75f;
static constexpr int     kArrowVolleyCount    = 9;
static constexpr float   kArrowVolleySpreadDegrees = 56.f;
static constexpr float   kPiercingMultiRadius = 2.0f;
static constexpr int     kPiercingMultiWaveCount = 10;
static constexpr int     kPiercingMultiBurstCount = 2;
static constexpr float   kPiercingMultiInterval = 0.06f;
static constexpr float   kPiercingMultiLifetime = 0.42f;
static constexpr float   kPiercingMultiHalfWidth = 2.5f;
static constexpr float   kPiercingMultiHalfHeight = 1.25f;
static constexpr float   kRagdollLimitGraceDecay = mu::pi; // 180 deg/sec

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

Game::Game() {
	// 스레드 풀 초기화
	std::cout << "----------[게임 초기화 설정]----------\n";
	std::cout << "스레드 풀에 사용할 스레드 수를 입력해 주세요.\n";
	std::cout << "컴퓨터의 물리 코어 수: " << numberOfPhysicalCores() << '\n';
	std::cout << "사용 가능한 물리 코어 수: " << numberOfPhysicalCores() - 1 << " (1개 - 메인 스레드)\n";
	std::cout << "스레드 수: ";

	std::size_t threadCnt{};
	threadCnt = 4u;
	// std::cin >> threadCnt;

	threadPool_.run(threadCnt);

	// GFX 객체 초기화
	gfx_.setupDXGI(D3D_FEATURE_LEVEL_12_1);
	gfx_.init();
	gfx_.createSwapChain();
	gfx_.setThreadPool(&threadPool_);


	assetManager_.loadGFXAssets(gfx_, assetConfigs_);
}

Game::~Game() {
	if (goblin_ && goblin_->ragdoll().isBuilt())
		goblin_->ragdoll().destroy(physicsWorld_);
}


void Game::setupStage() {
	const auto path = std::filesystem::path("../resources/levels/level.bin");
	auto ifs = std::ifstream(path, std::ios::binary);
	DISPLAY_ERROR_STR(ifs.good(), "[File I/O Error]: loadModelFromFile: "s + path.string() + " 파일을 열 수 없습니다."s, true);

	readHeadTag(ifs, "Level");
	const auto nodeCnt = readInteger(ifs, "NodeCnt");

	importNode(ifs);

	readTailTag(ifs, "Level");

	// 이후 레벨에서 사용하는 스카이박스 정보들을 읽어들일 수 있지만
	// 스카이박스 재질을 하나만 사용하므로 굳이 읽지 않는다.

	gSharedLog << "[Level Load] File I/O: 레벨 " << path << "로드 완료\n";

	dumpLog();

	skybox_.setModel(assetManager_.modelCube());
	skybox_.setSkyboxMaterial(assetManager_.skyboxMaterial());

	dirLight_.setOrient(mu::NQuat(mu::Degree(0.f), mu::Degree(160.f), mu::Degree(0.f)));
	dirLight_.color = mu::Vec3(0.8f, 0.8f, 0.8f);
	dirLight_.intensity = 2.f;
	dirLight_.type = PBRPipeline::LightData::Type::DirectionalLight;
	dirLight_.isMainDirectionalLight = true;

	camera_.setTargetObject( player_ );
	camera_.setOffsetFromTarget( mu::Vec3( 0.f, 1.8f, -2.5f ) );
	camera_.setOffsetTargetPivot( mu::Vec3(0.f, 1.f, 0.f));
	camera_.setPerspective( mu::Degree( 90.f ),
		static_cast<float>( gClientRect.right - gClientRect.left ) / ( gClientRect.bottom - gClientRect.top ),
		0.1f, 500.f
	);
	camera_.setPhysicsWorld(&physicsWorld_);

	//for (auto& cube : cubes_) {
	//	cube.enableBVRendering();
	//}

	player_->enableBVRendering();
	goblin_->enableBVRendering();

	setParticle();

	// 전투 시스템에 참가자 등록
	// 플레이어: 공격 hitbox 및 데미지 설정 (AI 쿨타임은 사용하지 않음)
	combatSystem_.registerCombatant(player_.get(),   { {1.5f, 1.5f, 1.5f}, 1.0f, 30, 500ms  });
	// 몬스터: 종류별로 공격 범위·데미지·쿨타임 차등 적용
	combatSystem_.registerCombatant(goblin_.get(),   { {1.2f, 1.5f, 1.2f}, 0.8f, 15, 2000ms });

	uiManager_.setScreenSize(
		static_cast<float>(gClientRect.right - gClientRect.left),
		static_cast<float>(gClientRect.bottom - gClientRect.top)
	);
	uiManager_.requestDebugResources(gfx_);
	auto* pLabel = static_cast<UI::Label*>(
		uiManager_.root()->addChild(std::make_unique<UI::Label>())
	);
	pLabel->name    = "hpLabel";
	pLabel->anchor  = UI::Anchors::Center; // 부모의 어느 점에 붙을 지
	pLabel->pivot   = UI::Pivots::Center;	 // 내 박스의 어느 점에 못을 걸지	
	pLabel->width   = UI::DimValue::px(1000.0f);
	pLabel->height  = UI::DimValue::px(500.0f);
	pLabel->offsetX = UI::DimValue::px( -225.f );
	pLabel->offsetY = UI::DimValue::px( -250.f );
	pLabel->setTextHAlign(UI::TextHAlign::Center);
	pLabel->setTextVAlign(UI::TextVAlign::Center);
	pLabel->setText(L"U: UI영역 표시\nEnter: 마우스 포인터 캡처\nSpace: 마우스 포인터 감추기\nWASD: 이동\nG: GBuffer 버퍼내용 순환(0=None, 1=Albedo, ..., 7=Depth)\nH: Hi-Z Cull ON/OFF\n좌클릭: 공격 ");
	pLabel->setFontSize(20.0f);
	//pLabel->setAutoSize( true );
	pLabel->setTextColor( 1.0f, 1.0f, 1.0f, 1.0f );

	playerHpBar_ = static_cast<UI::ProgressBar*>(
		uiManager_.root()->addChild(std::make_unique<UI::ProgressBar>())
	);
	playerHpBar_->name    = "playerHpBar";
	playerHpBar_->anchor  = UI::Anchors::TopLeft;
	playerHpBar_->pivot   = UI::Pivots::TopLeft;
	playerHpBar_->width   = UI::DimValue::px(300.f);
	playerHpBar_->height  = UI::DimValue::px(18.f);
	playerHpBar_->offsetX = UI::DimValue::px(20.f);
	playerHpBar_->offsetY = UI::DimValue::px(20.f);
	playerHpBar_->bgColor   = { 0.15f, 0.15f, 0.15f, 0.85f };
	playerHpBar_->fillColor = { 1.0f, 0.0f, 0.0f, 1.00f };
	playerHpBar_->setProgress(1.f);

	effectDropdown_ = static_cast<UI::Dropdown*>(
		uiManager_.root()->addChild(std::make_unique<UI::Dropdown>())
	);
	effectDropdown_->name    = "effectDropdown";
	effectDropdown_->anchor  = UI::Anchors::TopRight;
	effectDropdown_->pivot   = UI::Pivots::TopRight;
	effectDropdown_->offsetX = UI::DimValue::px(-12.f);
	effectDropdown_->offsetY = UI::DimValue::px(12.f);
	effectDropdown_->width   = UI::DimValue::px(180.f);
	effectDropdown_->setup({ "Slash Wave", "Slash Combo", "Slash 7", "Slash 1", "Spikes", "Crystals Front Attack", "AoE Slash Green", "Red Energy Explosion", "Crystals Cross Fade", "Arrow", "Arrow Volley", "Arrow Rain", "Energy Explosion Arrow", "Tornado Shot", "Piercing", "Piercing Slash", "Piercing Circle Slash", "Piercing Multi" });
	effectDropdown_->onSelectionChanged = [this](int idx) {
		currentEffect_ = static_cast<SwordEffect>(idx);
	};
	hiZStatsLabel_ = static_cast<UI::Label*>(
		uiManager_.root()->addChild(std::make_unique<UI::Label>())
	);
	hiZStatsLabel_->name    = "hiZStatsLabel";
	hiZStatsLabel_->anchor  = UI::Anchors::TopRight;
	hiZStatsLabel_->pivot   = UI::Pivots::TopRight;
	hiZStatsLabel_->width   = UI::DimValue::px(300.0f);
	hiZStatsLabel_->height  = UI::DimValue::px(60.0f);
	hiZStatsLabel_->offsetX = UI::DimValue::px(-40.f);
	hiZStatsLabel_->offsetY = UI::DimValue::px(50.f);
	hiZStatsLabel_->setTextHAlign(UI::TextHAlign::Trailing);
	hiZStatsLabel_->setTextVAlign(UI::TextVAlign::Top);
	hiZStatsLabel_->setFontSize(18.0f);
	hiZStatsLabel_->setTextColor(0.2f, 1.0f, 0.2f, 1.0f);
	hiZStatsLabel_->setText(L"HiZ: OFF");

	// Hi-Z occlusion culling용 renderObjectId 할당
	{
		u32t nextRenderObjId = 0u;
		goblin_->setRenderObjectId(nextRenderObjId++);
		for (auto& g : goblins_)
			g->setRenderObjectId(nextRenderObjId++);
		gfx_.setMaxRenderObjectId(nextRenderObjId - 1u);
	}

	setupMonsterHpBars();

	// Skill system: compile Lua skill files and build lookup tables.
	// objectById is a sparse array indexed by object ID (player=0, goblin=1).
	// vfxById maps vfxId to a pre-loaded ParticleEffect (vfxId 1 = sword_slash_1).
	{
		SkillCompiler compiler;
		const Skeleton* pSkeleton = (player_ && player_->model())
		    ? &player_->model()->skeleton : nullptr;
		auto skillAssets = compiler.compileAll("../resources/skills", pSkeleton);
		skillSystem_.registerAssets(std::move(skillAssets));

		skillObjectById_.assign(2, nullptr);
		if (player_) skillObjectById_[0] = player_.get();
		if (goblin_) skillObjectById_[1] = goblin_.get();

		skillVfxById_.assign(2, nullptr);
		skillVfxById_[1] = &swordSlash1Effect_;  // effects/sword_slash_1.json

		skillCtx_.objectById     = skillObjectById_.data();
		skillCtx_.objectByIdSize = static_cast<int>(skillObjectById_.size());
		skillCtx_.vfxById        = skillVfxById_.data();
		skillCtx_.vfxByIdSize    = static_cast<int>(skillVfxById_.size());
		skillCtx_.camera         = &camera_;
	}
}

void Game::setupMonsterHpBars() {
	auto registerBar = [&](Object* monster, float yOffset) {
		if (!monster) return;
		auto* bar = static_cast<UI::ProgressBar*>(
			uiManager_.root()->addChild(std::make_unique<UI::ProgressBar>())
		);
		bar->anchor    = UI::Anchors::TopLeft;
		bar->pivot     = UI::Pivots::TopLeft;
		bar->width     = UI::DimValue::px(80.f);
		bar->height    = UI::DimValue::px(8.f);
		bar->fillColor = { 0.9f, 0.15f, 0.1f, 1.f };
		bar->bgColor   = { 0.15f, 0.15f, 0.15f, 0.85f };
		bar->visible   = false;
		monsterHpBars_.emplace(monster->getId(), MonsterHpEntry{ monster, bar, yOffset });
	};

	registerBar(goblin_.get(),   2.5f);
}

void Game::setParticle()
{
	// ── Flame ────────────────────────────────────────────────────────────────
	{
		ps::ParticleSystemConfig cfg;
		cfg.main.lifetimeMin        = 0.5f;
		cfg.main.lifetimeMax        = 1.0f;
		cfg.main.speedMin           = 0.f;
		cfg.main.speedMax           = 0.3f;
		cfg.main.startColor         = { 1.f, 0.4f, 0.f, 1.f };
		cfg.main.startSizeMin       = 0.8f;
		cfg.main.startSizeMax       = 1.0f;
		cfg.main.startRotationMin   = 0.f;
		cfg.main.startRotationMax   = mu::pi * 2.f;
		cfg.main.gravityModifierMin = -0.3f;
		cfg.main.gravityModifierMax = 0.f;
		cfg.main.gravity            = { 0.f, -9.8f, 0.f };
		cfg.main.duration           = 0.f;   // no time limit

		cfg.emission.emitRate = 15.f;

		cfg.shape.type       = ps::ShapeModule::Type::Edge;
		cfg.shape.position   = { -6.f, 58.5f, -5.f };
		cfg.shape.direction  = { 0.f, 1.f, 0.f };
		cfg.shape.edgeLength = 1.5f;
		cfg.shape.edgeDir    = { 1.f, 0.f, 0.f };

		cfg.colorOverLifetime.enabled  = true;
		cfg.colorOverLifetime.gradient = ColorGradient::constant({ 1.f, 1.f, 1.f, 1.f });

		cfg.sizeOverLifetime.enabled   = true;
		cfg.sizeOverLifetime.sizeBegin = 1.0f;
		cfg.sizeOverLifetime.sizeEnd   = 1.0f;

		cfg.renderer.mode               = ps::RendererModule::Mode::Billboard;
		cfg.renderer.renderOrder        = 0;
		cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.flameTex(), .blend = ps::BlendMode::Additive };

		cfg.textureSheetAnimation.enabled   = true;
		cfg.textureSheetAnimation.tilesX    = 3;
		cfg.textureSheetAnimation.tilesY    = 3;
		cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
		cfg.textureSheetAnimation.cycles    = 1.f;

		flameParticleSystem_.startContinuous(cfg);
	}

	// ── Smoke ────────────────────────────────────────────────────────────────
	{
		ps::ParticleSystemConfig cfg;
		cfg.main.lifetimeMin        = 0.5f;
		cfg.main.lifetimeMax        = 1.0f;
		cfg.main.speedMin           = 0.5f;
		cfg.main.speedMax           = 2.f;
		cfg.main.startColor         = { 0.5f, 0.5f, 0.5f, 1.f };
		cfg.main.startSizeMin       = 1.f;
		cfg.main.startSizeMax       = 1.f;
		cfg.main.startRotationMin   = 0.f;
		cfg.main.startRotationMax   = mu::pi * 2.f;
		cfg.main.gravityModifierMin = -0.5f;
		cfg.main.gravityModifierMax = -0.2f;
		cfg.main.gravity            = { 0.f, -1.f, 0.f };
		cfg.main.duration           = 0.f;

		cfg.emission.emitRate = 10.f;

		cfg.shape.type       = ps::ShapeModule::Type::Edge;
		cfg.shape.position   = { -6.f, 58.5f, -5.f };
		cfg.shape.direction  = { 0.f, 1.f, 0.f };
		cfg.shape.edgeLength = 1.5f;
		cfg.shape.edgeDir    = { 1.f, 0.f, 0.f };

		cfg.colorOverLifetime.enabled  = true;
		cfg.colorOverLifetime.gradient = ColorGradient{
			.keys = {
				{ 0.0f, { 1.f,  1.f,  1.f,  0.f } },
				{ 0.5f, { 0.5f, 0.5f, 0.5f, 1.f } },
				{ 1.0f, { 0.f,  0.f,  0.f,  0.f } },
			}
		};

		cfg.sizeOverLifetime.enabled   = true;
		cfg.sizeOverLifetime.sizeBegin = 1.f;
		cfg.sizeOverLifetime.sizeEnd   = 1.f;

		cfg.renderer.mode               = ps::RendererModule::Mode::Billboard;
		cfg.renderer.renderOrder        = 1;
		cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.smokeTex() };

		cfg.textureSheetAnimation.enabled   = true;
		cfg.textureSheetAnimation.tilesX    = 3;
		cfg.textureSheetAnimation.tilesY    = 3;
		cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
		cfg.textureSheetAnimation.cycles    = 1.f;

		smokeParticleSystem_.startContinuous(cfg);
	}

	auto loadUnityParticleConfig = [](const std::filesystem::path& jsonPath,
	                                  std::string_view relativePath) {
		ps::ParticleSystemConfig cfg;
		loadParticleSystemConfigFromUnityJson(jsonPath, relativePath, cfg);
		return cfg;
	};

	// ── Sword Slash 1 effect ─────────────────────────────────────────────────
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Sword Slash 1_ParticleSystems.json",
			"Sword Slash 1"
		);
		cfg.renderer.pMesh = assetManager_.meshSlash3();
		cfg.renderer.pSubMesh = assetManager_.meshSlash3()->subMeshes.empty()
		                       ? nullptr
		                       : &assetManager_.meshSlash3()->subMeshes[0];
		cfg.renderer.mat = assetManager_.swordSlashMaterial();
		cfg.renderer.renderOrder = 2;
		swordSlash1Effect_.addSystem(cfg, ParticleEffect::PlayMode::Emit);
	}

	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Sword Slash 1_ParticleSystems.json",
			"Sword Slash 1/Smoke"
		);
		cfg.main.looping = false;  // Re-triggered manually per slash.
		cfg.renderer.mat = assetManager_.smokeBlendCGMaterial();
		swordSlash1Effect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
	}

	// ── Sword Slash Combo: five Sword Slash 1 layers, tuned only by rotation/burst
	{
		auto baseCfg = loadUnityParticleConfig(
			"../resources/effects/Sword Slash 1_ParticleSystems.json",
			"Sword Slash 1"
		);
		baseCfg.renderer.pMesh = assetManager_.meshSlash3();
		baseCfg.renderer.pSubMesh = assetManager_.meshSlash3()->subMeshes.empty()
		                           ? nullptr
		                           : &assetManager_.meshSlash3()->subMeshes[0];
		baseCfg.renderer.mat = assetManager_.swordSlashMaterial();
		baseCfg.renderer.renderOrder = 2;
		baseCfg.main.looping = false;
		baseCfg.main.startRotation3DEnabled = true;
		baseCfg.emission.enabled = true;
		baseCfg.emission.emitRate = 0.f;
		baseCfg.emission.rateOverDistance = 0.f;

		{
			auto cfg = baseCfg;
			cfg.main.startRotation3DMin = { 0.f, -2.44346094f, -0.62831855f };
			cfg.main.startRotation3DMax = cfg.main.startRotation3DMin;

			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.10f,
					.countMin = 1,
					.countMax = 1,
					.cycleCount = 1,
					.repeatInterval = 0.01f,
					.probability = 1.f,
				}
			};

			swordSlashComboEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
		}

		{
			auto cfg = baseCfg;
			cfg.main.startRotation3DMin = { 0.f, -2.44346094f, 0.31415927f };
			cfg.main.startRotation3DMax = cfg.main.startRotation3DMin;

			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.45f,
					.countMin = 1,
					.countMax = 1,
					.cycleCount = 1,
					.repeatInterval = 0.01f,
					.probability = 1.f,
				}
			};

			swordSlashComboEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
		}

		{
			auto cfg = baseCfg;
			cfg.main.startRotation3DMin = { 0.f, -2.44346094f, -0.20943952f };
			cfg.main.startRotation3DMax = cfg.main.startRotation3DMin;

			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.6f,
					.countMin = 1,
					.countMax = 1,
					.cycleCount = 1,
					.repeatInterval = 0.01f,
					.probability = 1.f,
				}
			};

			swordSlashComboEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
		}

		{
			auto cfg = baseCfg;
			cfg.main.startRotation3DMin = { 0.f, 2.44346094f, 0.59341192f };
			cfg.main.startRotation3DMax = cfg.main.startRotation3DMin;

			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 1.15f,
					.countMin = 1,
					.countMax = 1,
					.cycleCount = 1,
					.repeatInterval = 0.01f,
					.probability = 1.f,
				}
			};

			swordSlashComboEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
		}

		{
			auto cfg = baseCfg;
			cfg.main.startRotation3DMin = { 0.f, 0.0f, 0.f };
			cfg.main.startRotation3DMax = cfg.main.startRotation3DMin;

			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 1.15f,
					.countMin = 1,
					.countMax = 1,
					.cycleCount = 1,
					.repeatInterval = 0.01f,
					.probability = 1.f,
				}
			};

			swordSlashComboEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
		}
	}

	// ── Sword Slash 7 / Slash (mesh particle, manual emit) ───────────────────
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Slash_ParticleSystems.json",
			"Slash"
		);
		cfg.renderer.pMesh = assetManager_.meshSlash3();
		cfg.renderer.pSubMesh = assetManager_.meshSlash3()->subMeshes.empty()
		                       ? nullptr
		                       : &assetManager_.meshSlash3()->subMeshes[0];
		cfg.renderer.mat = assetManager_.swordSlash2Material();
		cfg.renderer.renderOrder = 2;
		swordSlash7Effect_.addSystem(cfg, ParticleEffect::PlayMode::Emit);
	}

	// ── Sword Slash 7 / Slashes (mesh particle, manual emit) ─────────────────
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Slashes_ParticleSystems.json",
			"Slashes"
		);
		cfg.renderer.pMesh = assetManager_.meshSlash3();
		cfg.renderer.pSubMesh = assetManager_.meshSlash3()->subMeshes.empty()
		                       ? nullptr
		                       : &assetManager_.meshSlash3()->subMeshes[0];
		cfg.renderer.mat = assetManager_.swordSlash2Material();
		cfg.renderer.renderOrder = 2;
		swordSlash7Effect_.addSystem(cfg, ParticleEffect::PlayMode::Emit);
	}

	// ── Spikes Attack effect ───────────────────────────────────────────────
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Spikes attack_ParticleSystems.json",
			"Spikes attack/Spikes"
		);
		cfg.renderer.pMesh = assetManager_.meshIceSpikes2();
		cfg.renderer.pSubMesh = assetManager_.meshIceSpikes2()->subMeshes.empty()
		                       ? nullptr
		                       : &assetManager_.meshIceSpikes2()->subMeshes[0];
		cfg.renderer.mat = assetManager_.spikesMaterial();
		cfg.colorOverLifetime.enabled = true;
		cfg.colorOverLifetime.gradient = ColorGradient{
			.keys = {
				{ 0.0f, { 1.f, 1.f, 1.f, 1.f } },
				{ 0.78f, { 1.f, 1.f, 1.f, 1.f } },
				{ 1.0f, { 1.f, 1.f, 1.f, 0.f } },
			}
		};
		spikesAttackEffect_.addSystem(cfg, ParticleEffect::PlayMode::Emit);
	}

	// ── Piercing effect ────────────────────────────────────────────────────
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/PS_VFX_Piercing_ParticleSystems.json",
			"PS_VFX_Piercing"
		);
		cfg.renderer.pMesh = assetManager_.meshVfxProjectile02();
		cfg.renderer.pSubMesh = assetManager_.meshVfxProjectile02()->subMeshes.empty()
		                       ? nullptr
		                       : &assetManager_.meshVfxProjectile02()->subMeshes[0];
		cfg.renderer.mat = assetManager_.piercingMaterial();
		// 이 VFX는 Unity에서 로컬 +X 축으로 발사/정렬되도록 제작됨 (VelocityModule.x=20).
		// 엔진 forward는 +Z이므로 이미터 방향과 메시 기준 회전을 -90° yaw 보정해
		// 로컬 +X → +Z 로 맞춘다. 이후 play()의 orient가 플레이어 방향으로 회전시킨다.
		const mu::Mat4x4 piercingForwardFix = mu::rotateYH(mu::Degree(-90.f));
		cfg.shape.orientation    = piercingForwardFix;
		cfg.main.startRotation3D = piercingForwardFix;
		piercingEffect_.addSystem(cfg, ParticleEffect::PlayMode::Emit);
	}

	// ── PiercingMulti effect: player forward에 수직인 전방 벽에 비처럼 분포해 2발씩 10웨이브 stab ──
	{
		const mu::Mat4x4 piercingForwardFix = mu::rotateYH(mu::Degree(-90.f));
		const mu::Vec2 offsets[kPiercingMultiWaveCount][kPiercingMultiBurstCount] = {
			{ { -2.24f,  1.04f }, {  0.88f, -0.62f } },
			{ { -0.62f, -1.08f }, {  2.18f,  0.38f } },
			{ { -1.78f,  0.24f }, {  1.46f,  1.12f } },
			{ { -2.38f, -0.72f }, {  0.34f,  0.82f } },
			{ { -0.96f,  1.18f }, {  2.34f, -0.24f } },
			{ { -1.42f, -0.38f }, {  0.72f, -1.16f } },
			{ { -2.08f,  0.62f }, {  1.88f,  0.92f } },
			{ { -0.28f, -0.84f }, {  2.48f, -1.02f } },
			{ { -2.46f,  0.06f }, {  0.18f,  1.24f } },
			{ { -1.12f, -1.22f }, {  1.24f,  0.14f } },
		};

		for (int wave = 0; wave < kPiercingMultiWaveCount; ++wave) {
			for (int lane = 0; lane < kPiercingMultiBurstCount; ++lane) {
				auto cfg = loadUnityParticleConfig(
					"../resources/effects/PS_VFX_Piercing_ParticleSystems.json",
					"PS_VFX_Piercing"
				);
				cfg.renderer.pMesh = assetManager_.meshVfxProjectile02();
				cfg.renderer.pSubMesh = assetManager_.meshVfxProjectile02()->subMeshes.empty()
				                       ? nullptr
				                       : &assetManager_.meshVfxProjectile02()->subMeshes[0];
				cfg.renderer.mat = assetManager_.piercingMaterial();

				cfg.main.startRotation3D = piercingForwardFix;
				cfg.main.speedMin        = 0.f;
				cfg.main.speedMax        = 0.f;
				cfg.main.lifetimeMin     = kPiercingMultiLifetime;
				cfg.main.lifetimeMax     = kPiercingMultiLifetime;
				cfg.main.looping         = false;
				cfg.main.duration        = kPiercingMultiInterval * static_cast<float>(kPiercingMultiWaveCount - 1) + 0.1f;
				cfg.main.maxParticles    = 4;

				cfg.velocityOverLifetime.enabled      = true;
				cfg.velocityOverLifetime.useCurves    = false;
				cfg.velocityOverLifetime.inWorldSpace = false;
				cfg.velocityOverLifetime.linear       = { 0.f, 0.f, 25.f };
				cfg.velocityOverLifetime.orbital      = { 0.f, 0.f, 0.f };
				cfg.velocityOverLifetime.radial       = 0.f;
				cfg.velocityOverLifetime.drag         = 0.f;

				cfg.shape.type        = ps::ShapeModule::Type::Box;
				cfg.shape.position    = {
					std::clamp(offsets[wave][lane].x(), -kPiercingMultiHalfWidth, kPiercingMultiHalfWidth),
					std::clamp(offsets[wave][lane].y(), -kPiercingMultiHalfHeight, kPiercingMultiHalfHeight),
					0.f
				};
				cfg.shape.rotation    = { 0.f, 0.f, 0.f };
				cfg.shape.orientation = mu::Mat4x4{};
				cfg.shape.direction   = { 0.f, 0.f, 1.f };
				cfg.shape.boxSize     = { 0.56f, 0.42f, 0.f };

				cfg.emission.enabled  = true;
				cfg.emission.emitRate = 0.f;
				cfg.emission.bursts   = {
					ps::EmissionModule::Burst{
						.time = kPiercingMultiInterval * static_cast<float>(wave),
						.countMin = 1, .countMax = 1, .cycleCount = 1
					}
				};

				piercingMultiEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous, 4);
			}
		}
	}

	// ── PiercingSlash effect (Vefects SH_VFX_Vefects_Slash_BIRP_New) ──────
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/PS_VFX_Slash_ParticleSystems.json",
			"PS_VFX_Slash"
		);
		cfg.renderer.pMesh = assetManager_.meshVfxSlash01HD();
		cfg.renderer.pSubMesh = assetManager_.meshVfxSlash01HD()->subMeshes.empty()
		                       ? nullptr
		                       : &assetManager_.meshVfxSlash01HD()->subMeshes[0];
		cfg.renderer.mat = assetManager_.piercingSlashMaterial();
		cfg.main.simulationSpeed = 2.0f;
		piercingSlashEffect_.addSystem(cfg, ParticleEffect::PlayMode::Emit);
	}

	// ── PiercingCircleSlash effect (same shader as PiercingSlash, Circle variant)
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/PS_VFX_PiercingCircleSlash_ParticleSystems.json",
			"PS_VFX_Slash"
		);
		cfg.renderer.pMesh = assetManager_.meshVfxSlash01HD();
		cfg.renderer.pSubMesh = assetManager_.meshVfxSlash01HD()->subMeshes.empty()
		                       ? nullptr
		                       : &assetManager_.meshVfxSlash01HD()->subMeshes[0];
		cfg.renderer.mat = assetManager_.piercingCircleSlashMaterial();
		cfg.main.startSizeMin *= 2.0f;
		cfg.main.startSizeMax *= 2.0f;
		cfg.rotationOverLifetime.enabled = true;
		cfg.rotationOverLifetime.useCurves = false;
		cfg.rotationOverLifetime.separateAxes = false;
		cfg.rotationOverLifetime.angularVelocityMin = 6.2831853f * 2.0f;  // 2 rev/sec (local Z)
		cfg.rotationOverLifetime.angularVelocityMax = 6.2831853f * 2.0f;
		piercingCircleSlashEffect_.addSystem(cfg, ParticleEffect::PlayMode::Emit);
	}

	// ── Crystals Front Attack effect ──────────────────────────────────────────
	{
		const std::filesystem::path crystalsJson =
			"../resources/effects/Crystals front attack_ParticleSystems.json";

		// parent: 7-burst trigger particles (invisible, just fires sub-emitter birth events)
		{
			auto cfg = loadUnityParticleConfig(crystalsJson, "Crystals front attack");
			cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
			cfg.renderer.mat  = ps::MatUnlit{ .mainTex = nullptr, .blend = ps::BlendMode::Additive };
			cfg.main.looping  = false;
			// importer does not parse subEmitters yet — set manually
			cfg.subEmitters.enabled = true;
			cfg.subEmitters.subEmitters = { {
				.event           = ps::SubEmittersModule::Event::Birth,
				.emitProbability = 1.f,
				.inheritSize     = true,
			} };
			crystalsFrontAttackEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);  // idx 0
		}

		// child: crystal pillars (StretchedBillboard in Unity)
		{
			auto cfg = loadUnityParticleConfig(crystalsJson, "Crystals front attack/Crystals");
			cfg.renderer.mat  = ps::MatUnlit{
				.mainTex = assetManager_.crystalFree1Tex(),
				.blend = ps::BlendMode::Alpha,
				.color = { 1.15271747f, 1.1794312f, 1.41421354f, 1.f }
			};
			crystalsFrontAttackEffect_.addSystem(cfg, ParticleEffect::PlayMode::Emit);  // idx 1
		}

		// parent[0] subEmitters[0] (Crystals entry) -> child[1]
		crystalsFrontAttackEffect_.bindSubEmitter(0, 0, 1, true);
	}

	// ── AoE Slash Green effect ────────────────────────────────────────────────
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/AoE slash green_ParticleSystems.json",
			"AoE slash green"
		);
		cfg.main.looping      = false;
		cfg.main.startColor   = { 1.f, 1.f, 1.f, 1.f };
		cfg.renderer.mode     = ps::RendererModule::Mode::Billboard;
		cfg.renderer.mat      = ps::MatUnlit{ .mainTex = assetManager_.circle2Tex(), .blend = ps::BlendMode::Additive };
		aoESlashGreenEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
	}
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/AoE slash green_ParticleSystems.json",
			"AoE slash green/Slash"
		);
		cfg.main.looping        = false;
		cfg.main.startColor     = { 1.f, 1.f, 1.f, 0.5f };
		cfg.renderer.mode       = ps::RendererModule::Mode::StretchedBillboard;
		cfg.renderer.lengthScale  = 5.f;
		cfg.renderer.mat        = ps::MatUnlit{ .mainTex = assetManager_.slashTex(), .blend = ps::BlendMode::Additive };
		aoESlashGreenEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
	}
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/AoE slash green_ParticleSystems.json",
			"AoE slash green/RotatePart"
		);
		cfg.main.looping    = false;
		cfg.renderer.mode   = ps::RendererModule::Mode::Billboard;
		cfg.renderer.mat    = ps::MatUnlit{ .mainTex = assetManager_.slashTex(), .blend = ps::BlendMode::Additive };
		aoESlashGreenEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
	}

	// ── Slash Wave effect ─────────────────────────────────────────────────────
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Slash wave_ParticleSystems.json",
			"Slash wave"
		);
		cfg.main.looping = false;
		cfg.renderer.pMesh = assetManager_.meshHalfTrail();
		cfg.renderer.pSubMesh = assetManager_.meshHalfTrail()->subMeshes.empty()
		                       ? nullptr
		                       : &assetManager_.meshHalfTrail()->subMeshes[0];
		cfg.renderer.mat = assetManager_.twoSidesMaterial();
		slashWaveEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
	}
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Slash wave_ParticleSystems.json",
			"Slash wave/SlashPath"
		);
		cfg.main.looping = false;
		cfg.shape.direction = { 0.f, 0.f, 1.f };
		cfg.renderer.pMesh = assetManager_.meshSlashWave();
		cfg.renderer.pSubMesh = assetManager_.meshSlashWave()->subMeshes.empty()
		                       ? nullptr
		                       : &assetManager_.meshSlashWave()->subMeshes[0];
		cfg.renderer.mat = assetManager_.slashPathMaterial();
		slashWaveEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
	}

	// ── 발 본 인덱스 탐색 (흙먼지 VFX용) ────────────────────────────────────
	const auto& playerSkeleton = player_->model()->skeleton;
	if ( playerSkeleton.bones ) {
		for ( const auto& bone : *playerSkeleton.bones ) {
			if ( bone.name == "foot_l" )       footBoneIdxLeft_  = bone.boneIdx;
			else if ( bone.name == "foot_r" )  footBoneIdxRight_ = bone.boneIdx;
		}
	}
	if ( footBoneIdxLeft_ < 0 || footBoneIdxRight_ < 0 ) {
		gSharedLog << "[Dust VFX] Warning: foot bones not found. Dumping all bone names:\n";
		if ( playerSkeleton.bones ) {
			for ( const auto& bone : *playerSkeleton.bones )
				gSharedLog << "  bone[" << bone.boneIdx << "] = \"" << bone.name << "\"\n";
		}
	}

	// ── Dust (foot impact VFX) ────────────────────────────────────────────────
	{
		ps::ParticleSystemConfig cfg;
		cfg.main.lifetimeMin        = 0.3f;
		cfg.main.lifetimeMax        = 0.6f;
		cfg.main.speedMin           = 0.3f;
		cfg.main.speedMax           = 0.8f;
		cfg.main.startColor         = { 0.55f, 0.4f, 0.25f, 0.8f };
		cfg.main.startSizeMin       = 0.8f;
		cfg.main.startSizeMax       = 1.2f;
		cfg.main.startRotationMin   = 0.f;
		cfg.main.startRotationMax   = mu::pi * 2.f;
		cfg.main.gravityModifierMin = 0.f;
		cfg.main.gravityModifierMax = 0.05f;
		cfg.main.gravity            = { 0.f, -9.8f, 0.f };

		cfg.shape.type      = ps::ShapeModule::Type::Cone;
		cfg.shape.coneAngle = 1.2f;
		cfg.shape.direction = { 0.f, 1.f, 0.f };

		cfg.colorOverLifetime.enabled  = true;
		cfg.colorOverLifetime.gradient = ColorGradient{
			.keys = {
				{ 0.0f, { 1.f, 1.f, 1.f, 0.f } },
				{ 0.2f, { 1.f, 1.f, 1.f, 1.f } },
				{ 1.0f, { 0.7f, 0.7f, 0.7f, 0.f } },
			}
		};

		cfg.sizeOverLifetime.enabled   = true;
		cfg.sizeOverLifetime.sizeBegin = 0.3f;
		cfg.sizeOverLifetime.sizeEnd   = 0.8f;

		cfg.renderer.mode               = ps::RendererModule::Mode::Billboard;
		cfg.renderer.renderOrder        = 1;
		cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.smokeTex() };

		cfg.textureSheetAnimation.enabled   = true;
		cfg.textureSheetAnimation.tilesX    = 3;
		cfg.textureSheetAnimation.tilesY    = 3;
		cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
		cfg.textureSheetAnimation.cycles    = 1.f;

		dustParticleSystem_.init(cfg);
	}

	// ── Red Energy Explosion effect ──────────────────────────────────────────
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Red energy explosion_ParticleSystems.json",
			"Red energy explosion/Core"
		);
		cfg.main.looping         = false;
		cfg.renderer.mode        = ps::RendererModule::Mode::Billboard;
		cfg.renderer.mat         = ps::MatUnlit{ .mainTex = assetManager_.stoneTex(), .blend = ps::BlendMode::Additive };
		redEnergyExplosionEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
	}
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Red energy explosion_ParticleSystems.json",
			"Red energy explosion/Trails"
		);
		cfg.main.looping         = false;
		cfg.renderer.mode        = ps::RendererModule::Mode::Billboard;
		cfg.renderer.mat         = ps::MatUnlit{ .mainTex = nullptr, .blend = ps::BlendMode::Alpha };

		cfg.trail.enabled              = true;
		cfg.trail.material.mainTex     = assetManager_.trail67Tex();
		cfg.trail.material.blend       = ps::BlendMode::Additive;
		redEnergyExplosionEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
	}
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Red energy explosion_ParticleSystems.json",
			"Red energy explosion/Circle"
		);
		cfg.main.looping                = false;
		cfg.main.startColor             = {
			cfg.main.startColor.x() * 2.3773584f,
			cfg.main.startColor.y() * 2.3773584f,
			cfg.main.startColor.z() * 2.3773584f,
			cfg.main.startColor.w()
		};
		cfg.renderer.mode               = ps::RendererModule::Mode::Billboard;
		cfg.renderer.mat                = ps::MatUnlit{ .mainTex = assetManager_.circleTex(), .blend = ps::BlendMode::Alpha };
		redEnergyExplosionEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);
	}
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Red energy explosion_ParticleSystems.json",
			"Red energy explosion/ShockWaveIn"
		);
		cfg.main.looping = false;
		cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
		cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.circleTex(), .blend = ps::BlendMode::Alpha };

		redEnergyExplosionEffect_.addSystem( cfg, ParticleEffect::PlayMode::Continuous );
	}
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Red energy explosion_ParticleSystems.json",
			"Red energy explosion/ShockWave"
		);
		cfg.main.looping = false;
		cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
		cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.circleTex(), .blend = ps::BlendMode::Alpha };

		redEnergyExplosionEffect_.addSystem( cfg, ParticleEffect::PlayMode::Continuous );
	}
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Red energy explosion_ParticleSystems.json",
			"Red energy explosion/Smoke"
		);
		cfg.main.looping = false;
		cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
		cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.smoke26Tex(), .blend = ps::BlendMode::Alpha };

		redEnergyExplosionEffect_.addSystem( cfg, ParticleEffect::PlayMode::Continuous );
	}
	{
		auto cfg = loadUnityParticleConfig(
			"../resources/effects/Red energy explosion_ParticleSystems.json",
			"Red energy explosion/Flash"
		);
		cfg.main.looping = false;
		cfg.renderer.mode = ps::RendererModule::Mode::StretchedBillboard;
		cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.flashTex(), .blend = ps::BlendMode::Alpha };

		redEnergyExplosionEffect_.addSystem( cfg, ParticleEffect::PlayMode::Continuous );
	}
	// Crystal Cross Fade effect: parent trigger + Crystals sub-emitter only.
	{
		const std::filesystem::path crystalsCrossFadeJson =
			"../resources/effects/Crystals crossfade 2_ParticleSystems.json";

		{
			auto cfg = loadUnityParticleConfig(crystalsCrossFadeJson, "Crystals crossfade 2");
			cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
			cfg.renderer.mat  = ps::MatUnlit{ .mainTex = nullptr, .blend = ps::BlendMode::Additive };
			cfg.main.looping  = false;
			cfg.subEmitters.enabled = true;
			cfg.subEmitters.subEmitters = { {
				.event           = ps::SubEmittersModule::Event::Birth,
				.emitProbability = 1.f,
				.inheritSize     = true,
			} };
			crystalsCrossFadeEffect_.addSystem(cfg, ParticleEffect::PlayMode::Continuous);  // idx 0
		}

		{
			auto cfg = loadUnityParticleConfig(crystalsCrossFadeJson, "Crystals crossfade 2/Crystals");
			cfg.renderer.mode = ps::RendererModule::Mode::StretchedBillboard;
			cfg.renderer.mat  = ps::MatUnlit{
				.mainTex = assetManager_.crystalFree1Tex(),
				.blend = ps::BlendMode::Alpha,
				.color = { 1.15271747f, 1.1794312f, 1.41421354f, 1.f }
			};
			crystalsCrossFadeEffect_.addSystem(cfg, ParticleEffect::PlayMode::Emit);  // idx 1
		}

		crystalsCrossFadeEffect_.bindSubEmitter(0, 0, 1);
	// ── Arrow Effect (Muzzle → mesh flight → Hit) ───────────────────────────
		{
			// System 0: Arrow mesh (parent) — flies in player's forward direction
			{
				ps::ParticleSystemConfig cfg;
				cfg.main.lifetimeMin = 0.4f;
				cfg.main.lifetimeMax = 0.4f;
				cfg.main.speedMin = 40.f;
				cfg.main.speedMax = 40.f;
				cfg.main.startSizeMin = 0.3f;
				cfg.main.startSizeMax = 0.3f;
				cfg.main.startColor = { 1.f, 1.f, 1.f, 1.f };
				cfg.main.gravityModifierMin = 0.f;
				cfg.main.gravityModifierMax = 0.f;
				cfg.main.looping = false;
				cfg.main.duration = 0.f;

				cfg.emission.emitRate = 0.f;

				cfg.shape.type = ps::ShapeModule::Type::Point;
				cfg.shape.position = { 0.f, 0.f, 0.f };
				cfg.shape.direction = { 0.f, 0.f, 1.f };  // local +Z, rotated by player orient at play()

				cfg.renderer.mode = ps::RendererModule::Mode::Mesh;
				cfg.renderer.renderOrder = 2;
				cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.arrowTex(), .blend = ps::BlendMode::Alpha };
				cfg.renderer.pMesh = assetManager_.meshArrow();
				cfg.renderer.pSubMesh = assetManager_.meshArrow()->subMeshes.empty()
					? nullptr : &assetManager_.meshArrow()->subMeshes[0];

				// Sub-emitter index 0 = Birth (muzzle), index 1 = Death (hit)
				cfg.subEmitters.enabled = true;
				cfg.subEmitters.subEmitters = {
					ps::SubEmittersModule::SubEmitter{
						.event = ps::SubEmittersModule::Event::Birth,
						.emitProbability = 1.f,
						.emitCount = 1,
						.inheritVelocity = false,
						.inheritColor = false,
						.inheritSize = false,
					},
					ps::SubEmittersModule::SubEmitter{
						.event = ps::SubEmittersModule::Event::Death,
						.emitProbability = 1.f,
						.emitCount = 1,
						.inheritVelocity = false,
						.inheritColor = false,
						.inheritSize = false,
					},
				};

				arrowEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 32 );
			}

			// System 1: ArrowMuzzle — spawned at arrow birth position
			{
				ps::ParticleSystemConfig cfg;
				cfg.main.lifetimeMin = 0.4f;
				cfg.main.lifetimeMax = 0.4f;
				cfg.main.speedMin = 0.f;
				cfg.main.speedMax = 0.f;
				cfg.main.startSizeMin = 2.0f;
				cfg.main.startSizeMax = 2.0f;
				cfg.main.startColor = { 1.f, 1.f, 1.f, 1.f };
				cfg.main.looping = false;

				cfg.emission.emitRate = 0.f;
				cfg.emission.bursts = {
					ps::EmissionModule::Burst{
						.time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1
					}
				};

				cfg.shape.type = ps::ShapeModule::Type::Point;
				cfg.shape.position = { 0.f, 0.f, 0.f };

				cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
				cfg.renderer.renderOrder = 3;
				cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.arrowMuzzleTex(), .blend = ps::BlendMode::Additive };

				cfg.textureSheetAnimation.enabled = true;
				cfg.textureSheetAnimation.tilesX = 8;
				cfg.textureSheetAnimation.tilesY = 4;
				cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
				cfg.textureSheetAnimation.cycles = 1.f;

				arrowEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 16 );
			}

			// System 2: ArrowHit — spawned at arrow death position
			{
				ps::ParticleSystemConfig cfg;
				cfg.main.lifetimeMin = 0.4f;
				cfg.main.lifetimeMax = 0.4f;
				cfg.main.speedMin = 0.f;
				cfg.main.speedMax = 0.f;
				cfg.main.startSizeMin = 2.5f;
				cfg.main.startSizeMax = 2.5f;
				cfg.main.startColor = { 1.f, 1.f, 1.f, 1.f };
				cfg.main.looping = false;

				cfg.emission.emitRate = 0.f;
				cfg.emission.bursts = {
					ps::EmissionModule::Burst{
						.time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1
					}
				};

				cfg.shape.type = ps::ShapeModule::Type::Point;
				cfg.shape.position = { 0.f, 0.f, 0.f };

				cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
				cfg.renderer.renderOrder = 3;
				cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.arrowHitTex(), .blend = ps::BlendMode::Additive };

				cfg.textureSheetAnimation.enabled = true;
				cfg.textureSheetAnimation.tilesX = 8;
				cfg.textureSheetAnimation.tilesY = 4;
				cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
				cfg.textureSheetAnimation.cycles = 1.f;

				arrowEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 16 );
			}

			// System 0 Birth → System 1 (muzzle), System 0 Death → System 2 (hit)
			arrowEffect_.bindSubEmitter( 0, 0, 1 );
			arrowEffect_.bindSubEmitter( 0, 1, 2 );
		}
	}

	// Arrow Volley effect: one caster muzzle, then a simultaneous fan of Arrow meshes.
	{
		// Muzzle: reused once at the caster position.
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin = 0.4f;
			cfg.main.lifetimeMax = 0.4f;
			cfg.main.speedMin = 0.f;
			cfg.main.speedMax = 0.f;
			cfg.main.startSizeMin = 2.2f;
			cfg.main.startSizeMax = 2.2f;
			cfg.main.startColor = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.looping = false;

			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1
				}
			};

			cfg.shape.type = ps::ShapeModule::Type::Point;
			cfg.shape.position = { 0.f, 0.f, 0.f };

			cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
			cfg.renderer.renderOrder = 3;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.arrowMuzzleTex(), .blend = ps::BlendMode::Additive };

			cfg.textureSheetAnimation.enabled = true;
			cfg.textureSheetAnimation.tilesX = 8;
			cfg.textureSheetAnimation.tilesY = 4;
			cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
			cfg.textureSheetAnimation.cycles = 1.f;

			arrowVolleyMuzzleEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 16 );
		}

		const float angleStep = kArrowVolleySpreadDegrees
			/ static_cast<float>( kArrowVolleyCount - 1 );
		const float firstAngle = -kArrowVolleySpreadDegrees * 0.5f;
		for ( int i = 0; i < kArrowVolleyCount; ++i ) {
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin = 0.45f;
			cfg.main.lifetimeMax = 0.45f;
			cfg.main.speedMin = 38.f;
			cfg.main.speedMax = 38.f;
			cfg.main.startSizeMin = 0.3f;
			cfg.main.startSizeMax = 0.3f;
			cfg.main.startColor = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.gravityModifierMin = 0.f;
			cfg.main.gravityModifierMax = 0.f;
			cfg.main.looping = false;
			cfg.main.duration = 0.f;

			const auto yawOffset = mu::rotateYH( mu::Degree( firstAngle + angleStep * static_cast<float>( i ) ) );
			cfg.main.startRotation3D = yawOffset;

			cfg.emission.emitRate = 0.f;

			cfg.shape.type = ps::ShapeModule::Type::Point;
			cfg.shape.position = { 0.f, 0.f, 0.f };
			cfg.shape.direction = { 0.f, 0.f, 1.f };
			cfg.shape.orientation = yawOffset;

			cfg.renderer.mode = ps::RendererModule::Mode::Mesh;
			cfg.renderer.renderOrder = 2;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.arrowTex(), .blend = ps::BlendMode::Alpha };
			cfg.renderer.pMesh = assetManager_.meshArrow();
			cfg.renderer.pSubMesh = assetManager_.meshArrow()->subMeshes.empty()
				? nullptr : &assetManager_.meshArrow()->subMeshes[0];

			cfg.subEmitters.enabled = true;
			cfg.subEmitters.subEmitters = {
				ps::SubEmittersModule::SubEmitter{
					.event = ps::SubEmittersModule::Event::Death,
					.emitProbability = 1.f,
					.emitCount = 1,
					.inheritVelocity = false,
					.inheritColor = false,
					.inheritSize = false,
				},
			};

			arrowVolleyEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 32 );
		}

		// Shared ArrowHit system for every arrow in the volley.
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin = 0.4f;
			cfg.main.lifetimeMax = 0.4f;
			cfg.main.speedMin = 0.f;
			cfg.main.speedMax = 0.f;
			cfg.main.startSizeMin = 2.5f;
			cfg.main.startSizeMax = 2.5f;
			cfg.main.startColor = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.looping = false;

			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1
				}
			};

			cfg.shape.type = ps::ShapeModule::Type::Point;
			cfg.shape.position = { 0.f, 0.f, 0.f };

			cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
			cfg.renderer.renderOrder = 3;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.arrowHitTex(), .blend = ps::BlendMode::Additive };

			cfg.textureSheetAnimation.enabled = true;
			cfg.textureSheetAnimation.tilesX = 8;
			cfg.textureSheetAnimation.tilesY = 4;
			cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
			cfg.textureSheetAnimation.cycles = 1.f;

			arrowVolleyEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 32 );
		}

		const int arrowVolleyHitIdx = kArrowVolleyCount;
		for ( int i = 0; i < kArrowVolleyCount; ++i ) {
			arrowVolleyEffect_.bindSubEmitter( i, 0, arrowVolleyHitIdx );
		}
	}

	// Arrow Rain effect: play Muzzle once at the caster, then drop Arrow meshes from above.
	{
		// Muzzle: reused once at the caster position.
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin = 0.4f;
			cfg.main.lifetimeMax = 0.4f;
			cfg.main.speedMin = 0.f;
			cfg.main.speedMax = 0.f;
			cfg.main.startSizeMin = 2.0f;
			cfg.main.startSizeMax = 2.0f;
			cfg.main.startColor = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.looping = false;

			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1
				}
			};

			cfg.shape.type = ps::ShapeModule::Type::Point;
			cfg.shape.position = { 0.f, 0.f, 0.f };

			cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
			cfg.renderer.renderOrder = 3;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.arrowMuzzleTex(), .blend = ps::BlendMode::Additive };

			cfg.textureSheetAnimation.enabled = true;
			cfg.textureSheetAnimation.tilesX = 8;
			cfg.textureSheetAnimation.tilesY = 4;
			cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
			cfg.textureSheetAnimation.cycles = 1.f;

			arrowRainMuzzleEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 16 );
		}

		// System 0: Arrow mesh rain parent. Emits multiple arrows across a disc above the target.
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin = 0.34f;
			cfg.main.lifetimeMax = 0.34f;
			cfg.main.speedMin = 32.f;
			cfg.main.speedMax = 32.f;
			cfg.main.startSizeMin = 0.3f;
			cfg.main.startSizeMax = 0.3f;
			cfg.main.startColor = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.gravityModifierMin = 0.f;
			cfg.main.gravityModifierMax = 0.f;
			cfg.main.looping = false;
			cfg.main.duration = 1.2f;
			cfg.main.startRotation3D = mu::rotateXH( mu::Degree( 90.f ) );

			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.f, .countMin = 3, .countMax = 3, .cycleCount = 10, .repeatInterval = 0.12f
				}
			};

			cfg.shape.type = ps::ShapeModule::Type::Cone;
			cfg.shape.position = { 0.f, 11.f, 0.f };
			cfg.shape.orientation = mu::rotateXH( mu::Degree( 90.f ) );
			cfg.shape.coneRadius = kArrowRainRadius;
			cfg.shape.coneAngle = 0.f;
			cfg.shape.radiusThickness = 1.f;

			cfg.renderer.mode = ps::RendererModule::Mode::Mesh;
			cfg.renderer.renderOrder = 2;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.arrowTex(), .blend = ps::BlendMode::Alpha };
			cfg.renderer.pMesh = assetManager_.meshArrow();
			cfg.renderer.pSubMesh = assetManager_.meshArrow()->subMeshes.empty()
				? nullptr : &assetManager_.meshArrow()->subMeshes[0];

			cfg.subEmitters.enabled = true;
			cfg.subEmitters.subEmitters = {
				ps::SubEmittersModule::SubEmitter{
					.event = ps::SubEmittersModule::Event::Death,
					.emitProbability = 1.f,
					.emitCount = 1,
					.inheritVelocity = false,
					.inheritColor = false,
					.inheritSize = false,
				},
			};

			arrowRainEffect_.addSystem( cfg, ParticleEffect::PlayMode::Continuous, 64 );
		}

		// System 1: ArrowHit, reused where each arrow reaches the ground.
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin = 0.4f;
			cfg.main.lifetimeMax = 0.4f;
			cfg.main.speedMin = 0.f;
			cfg.main.speedMax = 0.f;
			cfg.main.startSizeMin = 2.5f;
			cfg.main.startSizeMax = 2.5f;
			cfg.main.startColor = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.looping = false;

			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1
				}
			};

			cfg.shape.type = ps::ShapeModule::Type::Point;
			cfg.shape.position = { 0.f, 0.f, 0.f };

			cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
			cfg.renderer.renderOrder = 3;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.arrowHitTex(), .blend = ps::BlendMode::Additive };

			cfg.textureSheetAnimation.enabled = true;
			cfg.textureSheetAnimation.tilesX = 8;
			cfg.textureSheetAnimation.tilesY = 4;
			cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
			cfg.textureSheetAnimation.cycles = 1.f;

			arrowRainEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 32 );
		}

		arrowRainEffect_.bindSubEmitter( 0, 0, 1 );
	}

	// ── EnergyExplosionArrow effect (Charge / Arrow / Hit / HitWhiteBG) ─────
	// Sequence: Charge Death → Arrow, Arrow Death → Hit + HitWhiteBG
	{
		// System 0: Charge — plays first, then spawns Arrow on death
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin  = 1.6f;
			cfg.main.lifetimeMax  = 1.6f;
			cfg.main.speedMin     = 0.f;
			cfg.main.speedMax     = 0.f;
			cfg.main.startSizeMin = 8.f;
			cfg.main.startSizeMax = 8.f;
			cfg.main.startColor   = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.looping      = false;

			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1
				}
			};

			cfg.shape.type     = ps::ShapeModule::Type::Point;
			cfg.shape.position = { 0.f, 0.f, 0.f };

			cfg.renderer.mode        = ps::RendererModule::Mode::Billboard;
			cfg.renderer.renderOrder = 3;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.EnergyExplosionChargeTex(), .blend = ps::BlendMode::Additive };

			cfg.textureSheetAnimation.enabled   = true;
			cfg.textureSheetAnimation.tilesX    = 8;
			cfg.textureSheetAnimation.tilesY    = 6;
			cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
			cfg.textureSheetAnimation.cycles    = 1.f;

			cfg.subEmitters.enabled = true;
			cfg.subEmitters.subEmitters = {
				ps::SubEmittersModule::SubEmitter{
					.event = ps::SubEmittersModule::Event::Death,
					.emitProbability = 1.f, .emitCount = 1,
				},
			};

			energyExplosionArrowEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 16 );
		}

		// System 1: Arrow — spawned on Charge Death, then spawns hit effects on death
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin = 0.6f;
			cfg.main.lifetimeMax = 0.6f;
			cfg.main.speedMin    = 40.f;
			cfg.main.speedMax    = 40.f;
			cfg.main.startSizeMin = 0.3f;
			cfg.main.startSizeMax = 0.3f;
			cfg.main.startColor   = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.gravityModifierMin = 0.f;
			cfg.main.gravityModifierMax = 0.f;
			cfg.main.looping = false;
			cfg.main.duration = 0.f;

			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1
				}
			};

			cfg.shape.type = ps::ShapeModule::Type::Point;
			cfg.shape.position = { 0.f, 0.f, 0.f };
			cfg.shape.direction = { 0.f, 0.f, 1.f };

			cfg.renderer.mode = ps::RendererModule::Mode::Mesh;
			cfg.renderer.renderOrder = 2;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.arrowTex(), .blend = ps::BlendMode::Alpha };
			cfg.renderer.pMesh = assetManager_.meshArrow();
			cfg.renderer.pSubMesh = assetManager_.meshArrow()->subMeshes.empty()
				? nullptr : &assetManager_.meshArrow()->subMeshes[0];
			cfg.subEmitters.enabled = true;
			cfg.subEmitters.subEmitters = {
				ps::SubEmittersModule::SubEmitter{
					.event = ps::SubEmittersModule::Event::Death,
					.emitProbability = 1.f,
					.emitCount = 1,
					.inheritVelocity = false,
					.inheritColor = false,
					.inheritSize = false,
				},
				ps::SubEmittersModule::SubEmitter{
					.event = ps::SubEmittersModule::Event::Death,
					.emitProbability = 1.f,
					.emitCount = 1,
					.inheritVelocity = false,
					.inheritColor = false,
					.inheritSize = false,
				},
			};

			energyExplosionArrowEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 32 );
		}

		// System 2: Hit — spawned on Arrow Death (8x6 sprite, Additive)
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin  = 1.6f;
			cfg.main.lifetimeMax  = 1.6f;
			cfg.main.speedMin     = 0.f;
			cfg.main.speedMax     = 0.f;
			cfg.main.startSizeMin = 10.0f;
			cfg.main.startSizeMax = 10.0f;
			cfg.main.startColor   = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.looping      = false;

			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1
				}
			};

			cfg.shape.type     = ps::ShapeModule::Type::Point;
			cfg.shape.position = { 0.f, 0.f, 0.f };

			cfg.renderer.mode        = ps::RendererModule::Mode::Billboard;
			cfg.renderer.renderOrder = 3;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.EnergyExplosionHitTex(), .blend = ps::BlendMode::Additive };

			cfg.textureSheetAnimation.enabled   = true;
			cfg.textureSheetAnimation.tilesX    = 8;
			cfg.textureSheetAnimation.tilesY    = 6;
			cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
			cfg.textureSheetAnimation.cycles    = 1.f;

			energyExplosionArrowEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 16 );
		}

		// System 3: HitWhiteBG — spawned on Arrow Death (8x6 sprite, Multiply)
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin  = 1.6f;
			cfg.main.lifetimeMax  = 1.6f;
			cfg.main.speedMin     = 0.f;
			cfg.main.speedMax     = 0.f;
			cfg.main.startSizeMin = 10.0f;
			cfg.main.startSizeMax = 10.0f;
			cfg.main.startColor   = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.looping      = false;

			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts = {
				ps::EmissionModule::Burst{
					.time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1
				}
			};

			cfg.shape.type     = ps::ShapeModule::Type::Point;
			cfg.shape.position = { 0.f, 0.f, 0.f };

			cfg.renderer.mode        = ps::RendererModule::Mode::Billboard;
			cfg.renderer.renderOrder = 3;
			cfg.renderer.mat = ps::MatUnlit{ .mainTex = assetManager_.EnergyExplosionHitWhiteBGTex(), .blend = ps::BlendMode::Multiply };

			cfg.textureSheetAnimation.enabled   = true;
			cfg.textureSheetAnimation.tilesX    = 8;
			cfg.textureSheetAnimation.tilesY    = 6;
			cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
			cfg.textureSheetAnimation.cycles    = 1.f;

			energyExplosionArrowEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 16 );
		}

		// Charge.Death → Arrow, Arrow.Death → Hit, Arrow.Death → HitWhiteBG
		energyExplosionArrowEffect_.bindSubEmitter( 0, 0, 1 );
		energyExplosionArrowEffect_.bindSubEmitter( 1, 0, 2 );
		energyExplosionArrowEffect_.bindSubEmitter( 1, 1, 3 );
	}

	// TornadoShot: same visual as Tornado Continuous, moved forward each frame via setOrigin().
	// Muzzle and Hit are separate one-shot effects fired at shot start/end.
	{
		const std::filesystem::path tornadoJson =
			"../resources/effects/Par_TornadoContinous_ParticleSystems.json";

		const ps::MatWindRing ringMat{
			.mainTex          = assetManager_.texWindRing(),
			.edgeFadePower    = 2.f,
			.edgeFadeStrength = 1.f,
			.color            = { 0.933f, 0.933f, 0.933f, 0.765f },
		};

		// tornadoShotEffect_: identical 4-system setup as tornadoEffect_
		{
			auto cfg = loadUnityParticleConfig( tornadoJson, "Par_TornadoContinous" );
			cfg.renderer.mode     = ps::RendererModule::Mode::Mesh;
			cfg.renderer.pMesh    = assetManager_.meshRing();
			cfg.renderer.pSubMesh = assetManager_.meshRing()->subMeshes.empty()
				? nullptr : &assetManager_.meshRing()->subMeshes[0];
			cfg.renderer.mat      = ringMat;
			tornadoShotEffect_.addSystem( cfg, ParticleEffect::PlayMode::Continuous );
		}
		{
			auto cfg = loadUnityParticleConfig( tornadoJson, "Par_TornadoContinous/Bottom" );
			cfg.renderer.mode     = ps::RendererModule::Mode::Mesh;
			cfg.renderer.pMesh    = assetManager_.meshRing();
			cfg.renderer.pSubMesh = assetManager_.meshRing()->subMeshes.empty()
				? nullptr : &assetManager_.meshRing()->subMeshes[0];
			cfg.renderer.mat      = ringMat;
			tornadoShotEffect_.addSystem( cfg, ParticleEffect::PlayMode::Continuous );
		}
		{
			auto cfg = loadUnityParticleConfig( tornadoJson, "Par_TornadoContinous/RingRise" );
			cfg.renderer.mode     = ps::RendererModule::Mode::Mesh;
			cfg.renderer.pMesh    = assetManager_.meshRing();
			cfg.renderer.pSubMesh = assetManager_.meshRing()->subMeshes.empty()
				? nullptr : &assetManager_.meshRing()->subMeshes[0];
			cfg.renderer.mat      = ringMat;
			tornadoShotEffect_.addSystem( cfg, ParticleEffect::PlayMode::Continuous );
		}
		{
			auto cfg = loadUnityParticleConfig( tornadoJson, "Par_TornadoContinous/Par_BurstParticles" );
			cfg.renderer.mode = ps::RendererModule::Mode::Billboard;
			cfg.renderer.mat  = ps::MatUnlit{
				.mainTex = assetManager_.texDotParticle(),
				.blend   = ps::BlendMode::Alpha
			};
			tornadoShotEffect_.addSystem( cfg, ParticleEffect::PlayMode::Continuous );
		}

		// tornadoMuzzleEffect_: one-shot billboard at shot origin
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin  = 0.5f;
			cfg.main.lifetimeMax  = 0.5f;
			cfg.main.speedMin     = 0.f;
			cfg.main.speedMax     = 0.f;
			cfg.main.startSizeMin = 2.5f;
			cfg.main.startSizeMax = 2.5f;
			cfg.main.startColor   = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.looping      = false;
			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts   = {
				ps::EmissionModule::Burst{ .time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1 }
			};
			cfg.shape.type = ps::ShapeModule::Type::Point;
			cfg.renderer.mode        = ps::RendererModule::Mode::Billboard;
			cfg.renderer.renderOrder = 3;
			cfg.renderer.mat         = ps::MatUnlit{
				.mainTex = assetManager_.windBulletMuzzleTex(),
				.blend   = ps::BlendMode::Additive
			};
			cfg.textureSheetAnimation.enabled   = true;
			cfg.textureSheetAnimation.tilesX    = 8;
			cfg.textureSheetAnimation.tilesY    = 6;
			cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
			cfg.textureSheetAnimation.cycles    = 1.f;
			tornadoMuzzleEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 16 );
		}

		// tornadoHitEffect_: one-shot billboard at shot destination
		{
			ps::ParticleSystemConfig cfg;
			cfg.main.lifetimeMin  = 0.5f;
			cfg.main.lifetimeMax  = 0.5f;
			cfg.main.speedMin     = 0.f;
			cfg.main.speedMax     = 0.f;
			cfg.main.startSizeMin = 3.0f;
			cfg.main.startSizeMax = 3.0f;
			cfg.main.startColor   = { 1.f, 1.f, 1.f, 1.f };
			cfg.main.looping      = false;
			cfg.emission.emitRate = 0.f;
			cfg.emission.bursts   = {
				ps::EmissionModule::Burst{ .time = 0.f, .countMin = 1, .countMax = 1, .cycleCount = 1 }
			};
			cfg.shape.type = ps::ShapeModule::Type::Point;
			cfg.renderer.mode        = ps::RendererModule::Mode::Billboard;
			cfg.renderer.renderOrder = 3;
			cfg.renderer.mat         = ps::MatUnlit{
				.mainTex = assetManager_.windBulletHitTex(),
				.blend   = ps::BlendMode::Additive
			};
			cfg.textureSheetAnimation.enabled   = true;
			cfg.textureSheetAnimation.tilesX    = 8;
			cfg.textureSheetAnimation.tilesY    = 6;
			cfg.textureSheetAnimation.animation = ps::TextureSheetAnimationModule::Animation::WholeSheet;
			cfg.textureSheetAnimation.cycles    = 1.f;
			tornadoHitEffect_.addSystem( cfg, ParticleEffect::PlayMode::Emit, 16 );
		}
	}

}

// Helper: configure a monster body as Dynamic with shared character properties.
static void setupMonsterBody(RigidBody& body, float mass) {
	body.setMotionType(MotionType::Dynamic);
	body.setMass(mass);
	body.setLinearDamping(20.f);
	body.setAngularDamping(25.f);
	body.setUprightStiffness(4000.f);
}

void Game::importNode(std::ifstream& ifs) {
	readHeadTag(ifs, "Node");
	const auto type = readText(ifs, "Type");
	const auto name = readText(ifs, "Name");

	gSharedLog << "[Level Load] 레벨 노드 " << name << " 로드 완료\n";

	readHeadTag(ifs, "LocalTRS");
	const auto localT = readVec3(ifs, "Position");
	const auto localR = readVec4(ifs, "Rotation");
	const auto localS = readVec3(ifs, "Scale");
	readTailTag(ifs, "LocalTRS");

	readHeadTag(ifs, "WorldTRS");
	const auto worldT = readVec3(ifs, "Position");
	const auto worldR = readVec4(ifs, "Rotation");
	const auto worldS = readVec3(ifs, "Scale");
	readTailTag(ifs, "WorldTRS");

	Object object{};
	object.setPos(DirectX::XMLoadFloat3(&worldT));
	object.setOrient(DirectX::XMLoadFloat4(&worldR));
	object.setScale(DirectX::XMLoadFloat3(&worldS));

	if (type == "Cube") {
		auto tmp = std::make_shared<Cube>(std::move(object));
		tmp->setModel(assetManager_.modelCube());
		importCube(ifs, *tmp);
	}
	else if (type == "PlayerStart") {
		if (!playerSpawned_) {
			player_ = std::make_shared<Player>(std::move(object));
			playerSpawned_ = true;
			importPlayerStart(ifs, *player_);
			physicsWorld_.registerBody(&player_->body(),
				[p = player_.get()]() { p->rebuildBodyBVH(); });
			player_->body().setUserData(player_.get());
		}
	}
	else if (type == "GoblinSpawner") {
		const float baseX = object.pos().x();
		const float baseY = object.pos().y();
		const float baseZ = object.pos().z();

		goblin_ = std::make_shared<Goblin>(std::move(object));
		importGoblinSpawner(ifs, *goblin_);
		physicsWorld_.registerBody(&goblin_->body(),
			[p = goblin_.get()]() { p->rebuildBodyBVH(); });
		goblin_->body().setUserData(goblin_.get());

		auto urd = std::uniform_real_distribution<float>(-80.f, 80.f);

		for (std::size_t i = 0; i < 1u; ++i) {
			auto& g = goblins_.emplace_back( std::make_shared<Goblin>() );
			g->setPos( mu::Vec3( DirectX::XMLoadFloat3(&worldT) )
				+ mu::Vec3( urd(gRandomEngine), urd(gRandomEngine) + 80.f, urd(gRandomEngine) )
			);
			g->setOrient(DirectX::XMLoadFloat4(&worldR));
			g->setScale(DirectX::XMLoadFloat3(&worldS));

			g->setModel(assetManager_.modelGoblin());
			g->setAnimBlender(animSystem_, assetManager_);
			g->setHp(90);
			g->setMaxHp(90);
			g->setId(1);
			setupMonsterBody(g->body(), 40.f);
			g->enableBVRendering();

			physicsWorld_.registerBody(&g->body(),
				[p = g.get()]() { p->rebuildBodyBVH(); }
			);
			g->body().setUserData(g.get());
		}
	}
	else if (type == "Terrain") {
		terrain_ = std::make_shared<TerrainObject>(std::move(object));
		importTerrain(ifs, *terrain_);
		terrain_->update(0ms, 1.f);
	}
	else {
		// no-op
	}

	const auto childCnt = readInteger(ifs, "ChildCnt");
	readHeadTag(ifs, "Children");
	for (int i = 0; i < childCnt; ++i) {
		importNode(ifs);
	}
	readTailTag(ifs, "Children");

	readTailTag(ifs, "Node");
}

void Game::importCube(std::ifstream& ifs, Cube& cube) {
	const auto meshName = readText(ifs, "Mesh");
	const auto materialSetName = readText(ifs, "MaterialSet");
	const auto materialSetIdx = readInteger(ifs, "MaterialSetIndex");

	cube.setMaterialSetIdx(materialSetIdx);
}

void Game::importPlayerStart(std::ifstream& ifs, Player& player) {
	player.setId(0);
	player.setModel(assetManager_.modelPlayer());
	player.setAnimBlender(animSystem_, assetManager_);
	player.setHp(100);
	player.setMaxHp(100);

	player.body().setMotionType(MotionType::Dynamic);
	player.body().setMass(80.f);
	player.body().setLinearDamping(kPlayerLinearDamping);
	player.body().setAngularDamping(25.f);
	player.body().setUprightStiffness(40000.f);

	//Equipment rifle{};
	//rifle.socketType = Bone::SocketType::RightHand;
	//rifle.object = std::make_unique<Object>();
	//rifle.object->setModel(assetManager_.modelRifle());
	//rifle.object->setScale(mu::Vec3(1.f, 1.f, 1.f));

	//player.equip(std::move(rifle));
}

void Game::importGoblinSpawner(std::ifstream& ifs, Goblin& goblin) {
	goblin.setModel(assetManager_.modelGoblin());
	goblin.setAnimBlender(animSystem_, assetManager_);
	goblin.setHp(90);
	goblin.setMaxHp(90);
	goblin.setId(1);
	setupMonsterBody(goblin.body(), 40.f);

	if (goblin.model() && goblin.model()->ragdollDef) {
		goblin.ragdoll().build(
			goblin.model()->skeleton,
			*goblin.model()->ragdollDef,
			physicsWorld_
		);
	}
}

void Game::importTerrain(std::ifstream& ifs, TerrainObject& terrain) {
	const auto manifestPath = readText(ifs, "ManifestPath");
	terrain.setTerrainData(assetManager_.terrain());

	// 지형 물리 바디 설정: Static body (위치는 importNode WorldTRS에서 설정됨)
	terrain.body().setMotionType(MotionType::Static);

	// Hi-Z Occlusion Occluder 설정
	terrain.activateOcclusion(true);

	// TerrainCollider 등록 (BVH 불필요 — heightField 직접 조회)
	const TerrainData* td = assetManager_.terrain();
	if (td && !td->heightField.empty())
		physicsWorld_.registerTerrain(&terrain.body(), &td->heightField);
}

// ---------------------------------------------------------------------------
// Physics test object helpers
// ---------------------------------------------------------------------------

void Game::spawnTestObject(int kind) {
    // Offset successive spawns sideways so structures don't overlap.
    const float lateralOffset = static_cast<float>(rdObjects_.size()) * 4.f;
    const mu::Vec3 base = player_->pos()
        + player_->forward() * 4.f
        + player_->right() * lateralOffset;

    PhysicsTestObject obj;
    switch (kind) {
    case 1: obj = makePendulum(base);       break;
    case 2: obj = makeDoublePendulum(base); break;
    case 3: obj = makeHingeDoor(base);      break;
    case 4: obj = makeConeTwistArm(base);   break;
    case 5: obj = makeConeTwistChain(base);    break;
    case 6: obj = makeHumanoidRagdoll(base);   break;
    case 7: obj = makeUpperBodyRagdoll(base);  break;
    case 8: obj = makeLowerBodyRagdoll(base);  break;
    default: return;
    }

    obj.activate(physicsWorld_);
    rdObjects_.push_back(std::move(obj));
    rdShowBodies_ = true;  // auto-enable visualization when anything is spawned

    // Ragdoll-style objects (kind >= 6) have branching or deep joint chains that
    // require more PGS iterations to converge.  Extra joint-only iterations are
    // cheap (contacts are unaffected) and clearTestObjects() resets them to 0.
    if (kind >= 6)
        physicsWorld_.setJointSolverExtraIterations(48);
}

void Game::clearTestObjects() {
    for (auto& obj : rdObjects_) obj.deactivate(physicsWorld_);
    rdObjects_.clear();
    rdFrozen_ = false;
    physicsWorld_.setSolverIterations(4);
    physicsWorld_.setJointSolverExtraIterations(0);
    physicsWorld_.setPositionSolveIterations(3);
    physicsWorld_.setSubStepCount(2);
}

void Game::fireImpulseRay() {
    if (rdObjects_.empty()) return;

    const mu::NVec3 dir{ camera_.at() - camera_.eye() };
    const Ray ray{ camera_.eye(), mu::Vec3(dir) * 100.f };

    RigidBody* hitBody = nullptr;
    mu::Vec3   hitHe{};
    float      hitT = FLT_MAX;

    for (auto& obj : rdObjects_) {
        for (int i = 0; i < static_cast<int>(obj.bodies.size()); ++i) {
            auto* b = obj.bodies[i].get();
            if (b->motionType() != MotionType::Dynamic) continue;
            const mu::Vec3 he = (i < static_cast<int>(obj.halfExtents.size()))
                ? obj.halfExtents[i] : mu::Vec3{ 0.15f, 0.15f, 0.15f };
            const auto hit = RaycastOBB(OBB{ b->pos(), he, b->orient() }, ray);
            if (hit.hit && hit.t < hitT) {
                hitT    = hit.t;
                hitBody = b;
                hitHe   = he;
            }
        }
    }

    if (hitBody) {
        hitBody->applyImpulse(mu::Vec3(dir) * rdImpulseStrength_, hitBody->pos());
        debugBVView_.push(OBB{ hitBody->pos(), hitHe, hitBody->orient() }, 400ms);
    }
}

// 게임의 업데이트는 다음 순서대로 이루어진다.
// 입력 처리
// 이벤트 처리
// 물리 업데이트 루틴
// 객체별 업데이트 루틴
// 애니메이션 업데이트
void Game::update(Milliseconds deltaTime) {
	// Per-frame skill dispatch context: evList and pTimer may change each frame.
	skillCtx_.evList = &eventList_;
	skillCtx_.pTimer = pTimer_;

	// 입력 처리
	processInput(deltaTime);

	// debug BV 갱신 (TTL 감소 + 소멸 조건 평가)
	debugBVView_.update(deltaTime);

	// 몬스터 AI 공격 처리
	// 쿨타임 감소 후 플레이어와 AABB 교차 시 EvAttack + EvHit 발생
	if (!playerDead_) {
		combatSystem_.update(deltaTime, player_->getId(), eventList_);
	}

	// Skill system: advance timers, fire timeline events, update hitboxes, detect collisions.
	if (!playerDead_) {
		skillSystem_.update(deltaTime, skillCtx_);
		if (skillDebugBV_) skillSystem_.renderDebugHitboxes(debugBVView_);
	}

	// 이벤트 처리
	for (auto pEvRaw : eventList_) {
		auto pEv = reinterpret_cast<BasicEvent*>(pEvRaw);
		switch (pEv->type) {
		case EventType::Hit:
			if (static_cast<EvHit*>(pEv)->targetId == goblin_->getId()) {
				goblin_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, goblin_.get());
				if (goblin_->hp() == 0) {
					holdEvent( eventList_, EvDeath(goblin_->getId()) );
				}
				if (auto it = monsterHpBars_.find(goblin_->getId()); it != monsterHpBars_.end())
					it->second.hpBarVisibleSeconds = 5.f;
			}
			else if (static_cast<EvHit*>(pEv)->targetId == player_->getId()) {
				player_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, player_.get());
				if (player_->hp() <= 0 && !playerDead_) {
					playerDead_ = true;
					holdEvent(eventList_, EvDeath(player_->getId()));
				}
			}
			break;

		case EventType::Attack: {
			auto* attack = static_cast<EvAttack*>(pEv);
			if (attack->attackerId == goblin_->getId()) {
				goblin_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, goblin_.get());
			}

			// 공격 발동 시 attack hitbox를 1500ms 동안 live 추적으로 렌더링
			if (auto spec = combatSystem_.queryAttackSpec(attack->attackerId)) {
				debugBVView_.pushLive(spec->obj, spec->halfExtent, spec->offsetFwd, 1500ms);
			}
		}
		break;

		case EventType::Death: {
			auto* death = static_cast<EvDeath*>(pEv);

			if (death->victimId == goblin_->getId()) {
				goblin_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, goblin_.get());
			}
			else if (death->victimId == player_->getId()) {
				player_->eventBus()->receive(pEv, deltaTime, eventList_, *pTimer_, player_.get());
			}

			break;
		}

		case EventType::SkillHit: {
			// Translate skill damage into an EvHit so existing HP/animation logic is reused.
			// Skip dead targets — EventType::Hit handler emits EvDeath when hp reaches 0.
			auto* sh = static_cast<EvSkillHit*>(pEv);
			if (sh->targetId == goblin_->getId() && !goblin_->isDead()) {
				const i32t newHp = std::max(goblin_->hp() - sh->damage, 0);
				holdEvent(eventList_, EvHit(sh->targetId, newHp));
				if (auto it = monsterHpBars_.find(sh->targetId); it != monsterHpBars_.end())
					it->second.hpBarVisibleSeconds = 5.f;
			}
			break;
		}

		case EventType::CameraShake:
			// Camera shake is not yet implemented; event is consumed silently.
			break;

		case EventType::VFXSpawn:
			// VFX spawn is handled directly by SkillSystem::processHitResults; no further action.
			break;

		default:
			break;
		}
	}

	// 물리 업데이트 루틴
	// 
	// 물리량 갱신은 게임 갱신과 다르게 고정 주기로 수행한다.
	// 이를 통해 너무 유동적인 delta time으로 인한 시뮬레이션의 불안정성과
	// 물리 업데이트의 성능적 비용 문제를 해결한다.
	// 물리 업데이트 주기는 physicUpdateInterval_ 변수에 저장된다.
	//
	// update 함수에서 physicUpdateAcc_ 변수를 통해
	// 물리량 갱신의 주기가 돌아왔는지 판단하고
	// 주기가 되었다면 물리량 갱신을 수행한다.
	const Seconds clampedDt = std::min(Seconds(deltaTime), kMaxPhysicsDeltaTime);
	physicUpdateAcc_ += clampedDt * rdDebugTimeScale_;

	const Seconds effectiveInterval = physicUpdateInterval * static_cast<float>(physicUpdateScaleK_);

	int physicsStepsDone = 0;
	while (physicUpdateAcc_ >= effectiveInterval
		   && physicsStepsDone < kMaxPhysicsStepsPerFrame) {
		physicsWorld_.step(effectiveInterval);
		physicUpdateAcc_ -= effectiveInterval;
		++physicsStepsDone;
	}

	if (physicsStepsDone >= kMaxPhysicsStepsPerFrame) {
		++consecutiveLagFrames_;
		consecutiveNonLagFrames_ = 0;
		if (consecutiveLagFrames_ >= kLagScaleUpFrames && physicUpdateScaleK_ < kMaxPhysicsScaleK) {
			++physicUpdateScaleK_;
			consecutiveLagFrames_ = 0;
		}
	} else {
		consecutiveLagFrames_ = 0;
		if (physicUpdateScaleK_ > 1) {
			if (++consecutiveNonLagFrames_ >= kLagScaleDownFrames) {
				--physicUpdateScaleK_;
				consecutiveNonLagFrames_ = 0;
			}
		} else {
			consecutiveNonLagFrames_ = 0;
		}
	}
	skipNextRender_ = (consecutiveLagFrames_ >= kRenderSkipLagFrames);

	// Keep test bodies frozen: re-zero velocities every frame to resist gravity/constraints.
	if (rdFrozen_)
		for (auto& obj : rdObjects_) obj.freezeAll();

	// Push test-body OBBs each frame with a short TTL so they update live.
	if (rdShowBodies_)
		for (auto& obj : rdObjects_) obj.visualize(debugBVView_, 32ms);

	// BV 충돌 색상 업데이트: 기본=초록, Terrain-Object=빨강, Object-Object=파랑
	if (physicsStepsDone > 0) {
		static const mu::Vec4 kColDefault{ 0.f, 1.f, 0.f, 1.f };
		static const mu::Vec4 kColTerrain{ 1.f, 0.f, 0.f, 1.f };
		static const mu::Vec4 kColObjObj { 0.f, 0.f, 1.f, 1.f };

		if (player_) player_->setBVColor(kColDefault);
		if (goblin_) goblin_->setBVColor(kColDefault);
		for (auto& g : goblins_) g->setBVColor(kColDefault);

		// 1패스: Terrain-Object (빨강)
		physicsWorld_.forEachContact([&](const ContactConstraint& cc) {
			auto* objA = static_cast<Object*>(cc.bodyA->userData());
			auto* objB = static_cast<Object*>(cc.bodyB->userData());
			if (objA && !objB) objA->setBVColor(kColTerrain);
			if (objB && !objA) objB->setBVColor(kColTerrain);
		});
		// 2패스: Object-Object (파랑, 빨강보다 우선)
		physicsWorld_.forEachContact([&](const ContactConstraint& cc) {
			auto* objA = static_cast<Object*>(cc.bodyA->userData());
			auto* objB = static_cast<Object*>(cc.bodyB->userData());
			if (objA && objB) {
				objA->setBVColor(kColObjObj);
				objB->setBVColor(kColObjObj);
			}
		});
	}

	// 객체별 업데이트 루틴
	//
	// 물리량 갱신 주기에 대해,
	// 마지막 물리량 갱신으로부터 얼마나 지났는지의 비율로
	// RenderState 갱신을 위한 PhysicState 보간 계수를 설정한다.
	// 게임 객체의 update 함수에 전달된다.
	const auto tPhysicInterpolation = physicUpdateAcc_ / effectiveInterval;

	player_->update(deltaTime, tPhysicInterpolation);
	goblin_->update(deltaTime, tPhysicInterpolation);
	for (auto& g : goblins_) {
		g->update(deltaTime, tPhysicInterpolation);
	}
	
	animSystem_.updatePriorities(
		std::chrono::duration_cast<Seconds>(deltaTime),
		player_->pos()
	);

	camera_.update(deltaTime);
	dirLight_.update(deltaTime);
	dirLight_.updateCSMCascades(camera_.view(), camera_.proj(), assetConfigs_.cascade, assetConfigs_.shadowMap);

	// 몬스터 HP바 위치 갱신 (layout() 이전에 offset 갱신해야 반영됨)
	{
		constexpr float kBarHalfWidth = 40.f;
		const float dtSec = std::chrono::duration<float>(deltaTime).count();
		for (auto& [id, entry] : monsterHpBars_) {
			if (!entry.monster || entry.monster->hp() <= 0) {
				entry.hpBar->visible = false;
				entry.hpBarVisibleSeconds = 0.f;
				continue;
			}
			entry.hpBarVisibleSeconds = std::max(0.f, entry.hpBarVisibleSeconds - dtSec);
			if (entry.hpBarVisibleSeconds <= 0.f) {
				entry.hpBar->visible = false;
				continue;
			}
			const mu::Vec3 barWorldPos = entry.monster->renderState().pos
				+ mu::Vec3{ 0.f, entry.worldYOffset, 0.f };

			float sx{}, sy{};
			const bool onScreen = worldToScreen(
				barWorldPos,
				camera_.view(), camera_.proj(),
				uiManager_.screenWidth(), uiManager_.screenHeight(),
				sx, sy
			);
			entry.hpBar->visible = onScreen;
			if (onScreen) {
				entry.hpBar->offsetX = UI::DimValue::px(sx - kBarHalfWidth);
				entry.hpBar->offsetY = UI::DimValue::px(sy);
				entry.hpBar->setProgress(
					static_cast<float>(entry.monster->hp()) /
					static_cast<float>(entry.monster->maxHp())
				);
			}
		}
	}

	if (hiZStatsLabel_) {
		if (gfx_.isHiZCullEnabled()) {
			const auto stats = gfx_.getHiZStats();
			wchar_t buf[64];
			swprintf_s(buf, 64, L"HiZ: ON  Visible %u / %u", stats.visible, stats.total);
			hiZStatsLabel_->setText(buf);
		} else {
			hiZStatsLabel_->setText(L"HiZ: OFF");
		}
	}

	uiManager_.layout();
	uiManager_.update( std::chrono::duration<float>(deltaTime).count(), gfx_, gfx_.defaultFont() );

	// 애니메이션 업데이트
	animSystem_.update(0.01s);

	// Ragdoll 활성화: 이번 프레임 사망 → finalXformData 확정 후 seed + activate
	// Ragdoll 동기화: 활성 ragdoll body 위치를 finalXformData에 덮어씀
	{
		auto activateRagdollIfPending = [&](Goblin& g) {
			if (!g.ragdollPendingActivation()) return;
			g.setRagdollPendingActivation(false);
			Ragdoll& rd = g.ragdoll();
			if (!rd.isBuilt() || !g.animBlender() || !g.model()) return;
			rd.seedFromFinalXforms(
				g.animBlender()->finalXformData(),
				g.model()->skeleton,
				g.renderState().world
			);
			rd.buildPassengers(g.model()->skeleton, g.animBlender()->finalXformData());

			// Read knockback velocity before unregistering the main body.
			const mu::Vec3 initVel = g.body().linearVel();

			rd.activate(physicsWorld_);
			physicsWorld_.unregisterBody(&g.body());

			// Apply death velocity so the ragdoll flies in the knockback direction.
			if (initVel.len2() > 0.01f) {
				for (auto& rb : rd.bones()) {
					if (rb.body) rb.body->setLinearVel(initVel);
				}
			}

			// Per-bone random noise impulse, biased toward the death velocity direction.
			constexpr float kNoiseBias = 0.6f;
			const mu::Vec3 velDir = (initVel.len2() > 0.01f)
			    ? mu::Vec3(mu::NVec3(initVel)) : mu::Vec3{};
			for (const auto& rb : rd.bones()) {
				if (rb.noiseImpulse <= 0.f || !rb.body) continue;
				mu::Vec3 rnd(rand(-1.f, 1.f), rand(-1.f, 1.f), rand(-1.f, 1.f));
				if (rnd.len2() < 1e-8f) rnd = mu::Vec3(0.f, 0.f, 1.f);
				mu::Vec3 dir = velDir * kNoiseBias + mu::Vec3(mu::NVec3(rnd)) * (1.f - kNoiseBias);
				if (dir.len2() < 1e-8f) dir = mu::Vec3(0.f, 0.f, 1.f);
				rb.body->applyImpulse(mu::Vec3(mu::NVec3(dir)) * rb.noiseImpulse, rb.body->pos());
			}
		};

		auto syncRagdollToAnim = [&](Goblin& g) {
			Ragdoll& rd = g.ragdoll();
			if (!rd.isActive() || !g.animBlender() || !g.model()) return;
			rd.syncToFinalXforms(
				g.animBlender()->finalXformData(),
				g.model()->skeleton,
				g.renderState().world
			);
			g.rebuildBodyBVH();
		};

		activateRagdollIfPending(*goblin_);
		syncRagdollToAnim(*goblin_);
	}

	// 발 흙먼지 방출
	if (footBoneIdxLeft_ >= 0 && footBoneIdxRight_ >= 0
		&& player_->renderState().animBlender) {
		auto* animBlender = static_cast<AnimBlenderPlayer*>(
			player_->renderState().animBlender.get());

		const auto vel = player_->velocity();
		const float hSpeed2 = vel.x() * vel.x() + vel.z() * vel.z();
		constexpr float kDustMinSpeed = 1.0f;

		if (animBlender->isRunning() && hSpeed2 >= kDustMinSpeed * kDustMinSpeed) {
			const auto duration = animBlender->runDuration();
			const auto currTime = animBlender->runAnimTime();
			const float currPhase = currTime / duration;
			const float prevPhase = prevAnimTimeRun_ / duration;

			constexpr float kLeftFootContact  = 0.0f;
			constexpr float kRightFootContact = 0.5f;

			auto crossedPhase = [&](float phase) -> bool {
				if (currPhase >= prevPhase) {
					return prevPhase < phase && currPhase >= phase;
				} else {
					return prevPhase < phase || currPhase >= phase;
				}
			};

			const auto& skeleton = player_->model()->skeleton;
			const auto& boneXforms = animBlender->finalXformData();
			const auto& world = player_->renderState().world;

			auto getBoneWorldPos = [&](int boneIdx) -> mu::Vec3 {
				const auto& bone = skeleton.bones->at(boneIdx);
				const mu::Mat4x4 boneToWorld = bone.toDress
					* boneXforms[boneIdx] * world;
				return mu::Vec3(mu::Vec4(0.f, 0.f, 0.f, 1.f) * boneToWorld);
			};

			if (crossedPhase(kLeftFootContact)) {
				dustParticleSystem_.config().shape.position = getBoneWorldPos(footBoneIdxLeft_);
				dustParticleSystem_.emit(4);
			}
			if (crossedPhase(kRightFootContact)) {
				dustParticleSystem_.config().shape.position = getBoneWorldPos(footBoneIdxRight_);
				dustParticleSystem_.emit(4);
			}

			prevAnimTimeRun_ = currTime;
		} else {
			prevAnimTimeRun_ = 0s;
		}
	}

	// 파티클
	flameParticleSystem_.update( deltaTime );
	smokeParticleSystem_.update( deltaTime );
	swordSlash1Effect_.update( deltaTime );
	swordSlash7Effect_.update( deltaTime );
	swordSlashComboEffect_.update( deltaTime );
	slashWaveEffect_.update( deltaTime );
	spikesAttackEffect_.update( deltaTime );
	piercingEffect_.update( deltaTime );
	piercingMultiEffect_.update( deltaTime );
	piercingSlashEffect_.update( deltaTime );
	piercingCircleSlashEffect_.update( deltaTime );
	crystalsFrontAttackEffect_.update( deltaTime );
	aoESlashGreenEffect_.update( deltaTime );
	dustParticleSystem_.update( deltaTime );
	redEnergyExplosionEffect_.update( deltaTime );
	crystalsCrossFadeEffect_.update( deltaTime );
	arrowEffect_.update( deltaTime );
	arrowVolleyMuzzleEffect_.update( deltaTime );
	arrowVolleyEffect_.update( deltaTime );
	arrowRainMuzzleEffect_.update( deltaTime );
	arrowRainEffect_.update( deltaTime );
	energyExplosionArrowEffect_.update( deltaTime );
	tornadoShotEffect_.update( deltaTime );
	tornadoMuzzleEffect_.update( deltaTime );
	tornadoHitEffect_.update( deltaTime );

	// Advance tornado shot origin forward each frame
	if ( tornadoShotActive_ ) {
		constexpr float kSpeed    = 10.f;
		constexpr float kDuration = 0.8f;
		const Seconds dt = deltaTime;
		tornadoShotElapsed_ += dt;
		tornadoShotPos_ = tornadoShotPos_ + tornadoShotDir_ * kSpeed * dt.count();
		tornadoShotEffect_.setOrigin( tornadoShotPos_, tornadoShotOrient_ );
		if ( tornadoShotElapsed_.count() >= kDuration ) {
			tornadoShotEffect_.stop();
			tornadoHitEffect_.play( tornadoShotPos_ );
			tornadoShotActive_   = false;
			tornadoShotElapsed_  = 0s;
		}
	}

	// UI 동기화
	if (playerHpBar_)
		playerHpBar_->setProgress(player_->hp() / 100.f);

	clearEvents(eventList_);
}

void Game::render() {
	if (skipNextRender_) {
		skipNextRender_ = false;
		return;
	}

	cullObjects();
	cullObjectsForShadow();

	debugBVView_.render(gfx_);
	player_->render(gfx_);
	goblin_->render(gfx_);
	for (auto& g : goblins_) {
		g->render(gfx_);
	}
	skybox_.render(gfx_);
	camera_.updateGFX(gfx_);
	dirLight_.render(gfx_);

	flameParticleSystem_.render( gfx_ );
	smokeParticleSystem_.render( gfx_ );
	swordSlash1Effect_.render( gfx_ );
	swordSlash7Effect_.render( gfx_ );
	swordSlashComboEffect_.render( gfx_ );
	slashWaveEffect_.render( gfx_ );
	spikesAttackEffect_.render( gfx_ );
	piercingEffect_.render( gfx_ );
	piercingMultiEffect_.render( gfx_ );
	piercingSlashEffect_.render( gfx_ );
	piercingCircleSlashEffect_.render( gfx_ );
	crystalsFrontAttackEffect_.render( gfx_ );
	aoESlashGreenEffect_.render( gfx_ );
	dustParticleSystem_.render( gfx_ );
	redEnergyExplosionEffect_.render( gfx_ );
	crystalsCrossFadeEffect_.render( gfx_ );
	arrowEffect_.render( gfx_ );
	arrowVolleyMuzzleEffect_.render( gfx_ );
	arrowVolleyEffect_.render( gfx_ );
	arrowRainMuzzleEffect_.render( gfx_ );
	arrowRainEffect_.render( gfx_ );
	energyExplosionArrowEffect_.render( gfx_ );
	tornadoShotEffect_.render( gfx_ );
	tornadoMuzzleEffect_.render( gfx_ );
	tornadoHitEffect_.render( gfx_ );

	uiManager_.render( gfx_ );

	// FrameData와 gfx_.render()는 렉 상황에도 항상 호출한다 (DX12 swapchain 동기화 유지).
	gfx_.addFrameData( PBRPipeline::FrameData{ .globalAmbient = mu::Vec3( 0.16f, 0.16f, 0.16f ) } );
	gfx_.addFrameData( PBRSkinnedPipeline::FrameData{ .globalAmbient = mu::Vec3( 0.16f, 0.16f, 0.16f ) } );
	gfx_.addFrameData( PBRDeferredPipeline::FrameData{ .globalAmbient = mu::Vec3( 0.16f, 0.16f, 0.16f ) } );
	gfx_.addFrameData( PBRDeferredSkinnedPipeline::FrameData{ .globalAmbient = mu::Vec3( 0.16f, 0.16f, 0.16f ) } );

	if (terrain_) {
		terrain_->render(gfx_);
		gfx_.addFrameData(TerrainPipeline::FrameData{ .globalAmbient = mu::Vec3(0.16f, 0.16f, 0.16f) });
		gfx_.addFrameData(TerrainDeferredPipeline::FrameData{ .globalAmbient = mu::Vec3(0.16f, 0.16f, 0.16f) });
	}

	gfx_.render();
	applyHiZCulling();
}

// 윈도우 프로시저에서 특정한 메시지 처리를 위임받는다.
LRESULT Game::receiveWndMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	// WM_INPUT 메시지
	// 마우스 움직임의 경우 WM_MOUSEMOVE 메시지 대신 이 메시지로 처리하는 것이
	// 정확하고 안정적인 처리가 가능하다.
	// 윈도우 경계로부터 영향을 받지 않고, 가상 커서 속도/가속도 설정을 무시하며,
	// Alt-Tab / 창 이동 후에도 상태 복구가 명확하다.
	// (DPI 스케일링 환경에서도 입력이 왜곡되지 않는다고 한다.)
	case WM_INPUT: {
		static auto sRawInputBuffer = std::vector<std::uint8_t>(256);
		UINT rawInputSize{};
		UINT rawInputResult{};

		// 입력 구조체 크기 수신
		rawInputResult = GetRawInputData( reinterpret_cast<HRAWINPUT>(lParam),
			RID_INPUT, nullptr, &rawInputSize, sizeof(RAWINPUTHEADER)
		);
		DISPLAY_ERROR_GLE(rawInputResult != -1, true);

		if (rawInputSize > sRawInputBuffer.size()) {
			sRawInputBuffer.resize(rawInputSize);
		}

		// 입력 구조체 내용 수신
		rawInputResult = GetRawInputData( reinterpret_cast<HRAWINPUT>(lParam),
			RID_INPUT, sRawInputBuffer.data(), &rawInputSize, sizeof(RAWINPUTHEADER)
		);
		DISPLAY_ERROR_GLE(rawInputResult == rawInputSize, true);

		auto ri = reinterpret_cast<const RAWINPUT*>(sRawInputBuffer.data());
		if (ri->header.dwType == RIM_TYPEMOUSE) {
			// 마우스에 대한 입력 내용이 상대 좌표여야 한다.
			DISPLAY_ERROR_STR( !(ri->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE),
				"[Input Error] Game::receiveWndMsg: 마우스 입력 장치가 게임과 호환되지 않습니다.\n"
				"RAWMOUSE의 플래그 중 MOUSE_MOVE_ABSOLUTE가 활성화되어있습니다.",
				true
			);

			// 마우스 이동량 기록
			mouseDeltaX_ += ri->data.mouse.lLastX;
			mouseDeltaY_ += ri->data.mouse.lLastY;
		}

		return 0;
	}

	// Alt+Tab 등으로 윈도우가 포커스를 잃었다가 되찾은 경우,
	// 커서와 관련된 플래그들을 읽어 커서 캡처, 커서 숨기기 등을 다시 수행한다.
	case WM_SETFOCUS:
		if (cursorCaptureEnabled_) {
			captureCursor();
		}
		if (!cursorShowEnabled_) {
			hideCursor();
		}
		break;

	// Alt+Tab 등으로 윈도우가 포커스를 잃은 경우
	// 커서와 관련된 플래그들을 읽어 커서 캡처 해제, 커서 보이기 등을 수행한다.
	// 다른 윈도우로 전환되었는데 커서가 보이지 않거나 안 움직여지면 곤란할 것이다.
	case WM_KILLFOCUS:
		if (cursorCaptureEnabled_) {
			releaseCursor();
		}
		if (cursorShowEnabled_) {
			showCursor();
		}
		break;

	case WM_SIZE:
		break;

	default:
		uiManager_.onWndMsg(msg, wParam, lParam);
		break;
	}

	return DefWindowProcA(hWnd, msg, wParam, lParam);
};

void Game::processInput(Milliseconds deltaTime) {
	if (GetForegroundWindow() != ghWnd) {
		return;
	}

	keyboardStatePrev_ = keyboardStateCurr_;
	DISPLAY_ERROR_GLE( GetKeyboardState(keyboardStateCurr_.data()), false );

	// 이동 가속도만 담당. 감속은 PhysicsWorld의 linearDamping(마찰)이 처리한다.
	// 속도 상한은 kPlayerMaxSpeed, 가속률은 kPlayerAccelRate (파일 상단 상수 참조).
	const auto moveXSign = !playerDead_ * ( (keyboardStateCurr_['D'] & 0x80) - (keyboardStateCurr_['A'] & 0x80) );
	const auto moveZSign = !playerDead_ * ( (keyboardStateCurr_['W'] & 0x80) - (keyboardStateCurr_['S'] & 0x80) );

	if (moveXSign || moveZSign) {
		// 'W'/'S' 입력으로 판정된 Z 부호는 플레이어의 forward 벡터,
		// 'D'/'A' 입력으로 판정된 X 부호는 플레이어의 right 벡터와 곱해 속도의 방향을 정한다.
		const auto moveDirection = mu::NVec3(
			static_cast<float>(moveXSign) * player_->right() + static_cast<float>(moveZSign) * player_->forward()
		);

		// x/z 방향으로만 가속. y(중력)는 물리 엔진이 담당.
		const auto fullVel = player_->velocity();
		const auto accel   = mu::Vec3(moveDirection) * (kPlayerAccelRate * Seconds(deltaTime).count());
		float newX = fullVel.x() + accel.x();
		float newZ = fullVel.z() + accel.z();

		// x/z 속도만 클램프 (y는 건드리지 않음).
		const float hSpd2 = newX * newX + newZ * newZ;
		if (hSpd2 > kPlayerMaxSpeed * kPlayerMaxSpeed) {
			const float scale = kPlayerMaxSpeed / std::sqrt(hSpd2);
			newX *= scale;
			newZ *= scale;
		}
		player_->setVelocity(mu::Vec3(newX, fullVel.y(), newZ));
	}

	// Q key: trigger SwordSlash skill on the player.
	if ( !playerDead_
		&& !uiManager_.needsCursor()
		&& (keyboardStateCurr_['Q'] & 0x80)
		&& !(keyboardStatePrev_['Q'] & 0x80)
		) {
		skillSystem_.startSkill("SwordSlash", player_->getId(), skillCtx_);
	}

	// H key: toggle skill hitbox BV debug rendering.
	if ( (keyboardStateCurr_['H'] & 0x80)
		&& !(keyboardStatePrev_['H'] & 0x80)
		) {
		skillDebugBV_ = !skillDebugBV_;
	}

	// 플레이어 공격: LButton 클릭 시 forward 방향 hitbox와 몬스터 AABB 교차 검사
	if ( !playerDead_
		&& !uiManager_.needsCursor()
		&& (keyboardStateCurr_[VK_LBUTTON] & 0x80)
		&& !(keyboardStatePrev_[VK_LBUTTON] & 0x80)
		) {
		combatSystem_.onPlayerAttack( player_->getId(), eventList_ );
		// 공격 발동 시 플레이어 attack hitbox를 1500ms 동안 live 추적으로 렌더링
		if ( auto spec = combatSystem_.queryAttackSpec( player_->getId() ) ) {
			debugBVView_.pushLive( spec->obj, spec->halfExtent, spec->offsetFwd, 1500ms );
		}

		// 검기 이펙트 emit: SlashPath가 플레이어 forward 방향으로 날아가도록 회전을 함께 전달한다.
		const auto slashPos = player_->renderState().pos
			+ player_->forward() * 1.f
			+ mu::Vec3( 0.f, 1.0f, 0.f );
		switch ( currentEffect_ ) {
		case SwordEffect::SlashCombo: swordSlashComboEffect_.play( slashPos ); break;
		case SwordEffect::Slash7:     swordSlash7Effect_.play( slashPos );     break;
		case SwordEffect::Slash1:     swordSlash1Effect_.play( slashPos );     break;
		case SwordEffect::SlashWave:  slashWaveEffect_.play( slashPos, player_->orient() );       break;
		case SwordEffect::Spikes:              spikesAttackEffect_.play( slashPos );          break;
		case SwordEffect::Piercing:            piercingEffect_.play( slashPos, player_->orient() );              break;
		case SwordEffect::PiercingSlash:       piercingSlashEffect_.play( slashPos, player_->orient() );         break;
		case SwordEffect::PiercingCircleSlash: piercingCircleSlashEffect_.play( slashPos, player_->orient() );   break;
		case SwordEffect::PiercingMulti: {
			const auto multiCenter = player_->renderState().pos
				+ player_->forward() * (kPiercingMultiRadius + 1.f)
				+ mu::Vec3( 0.f, 1.0f, 0.f );
			piercingMultiEffect_.play( multiCenter, player_->orient() );
			break;
		}
		case SwordEffect::CrystalsFrontAttack: {
			const auto crystalPos = player_->renderState().pos + player_->forward() * 1.f;
			const mu::Mat4x4 crystalOrient = mu::rotateYH( mu::Degree( -90.f ) ) * player_->orient().mat4();
			crystalsFrontAttackEffect_.play( crystalPos, crystalOrient, player_->forward() );
			break;
		}
		case SwordEffect::AoESlashGreen: {
			const auto aoePos = slashPos + player_->forward() * 5.5f;
			aoESlashGreenEffect_.play( aoePos, player_->orient(), player_->forward() );
			break;
		}
		case SwordEffect::Arrow: {
			const auto arrowOrigin = player_->renderState().pos
				+ mu::Vec3{ 0.f, 1.2f, 0.f }
				+ player_->forward() * 0.5f;
			arrowEffect_.play( arrowOrigin, player_->orient() );
			break;
		}
		case SwordEffect::ArrowVolley: {
			const auto volleyOrigin = player_->renderState().pos
				+ mu::Vec3{ 0.f, 1.2f, 0.f }
				+ player_->forward() * 0.6f;
			arrowVolleyMuzzleEffect_.play( volleyOrigin, player_->orient() );
			arrowVolleyEffect_.play( volleyOrigin, player_->orient() );
			break;
		}
		case SwordEffect::ArrowRain: {
			const auto muzzlePos = player_->renderState().pos + mu::Vec3{ 0.f, 1.2f, 0.f };
			auto rainCenter = player_->renderState().pos + player_->forward() * 6.5f;
			if ( terrain_ && !assetManager_.terrain()->heightField.empty() ) {
				const auto terrainPos = terrain_->renderState().pos;
				const float localX = rainCenter.x() - terrainPos.x();
				const float localZ = rainCenter.z() - terrainPos.z();
				const float groundY = terrainPos.y()
					+ assetManager_.terrain()->heightField.getHeightAt( localX, localZ );
				rainCenter = { rainCenter.x(), groundY, rainCenter.z() };
			}
			arrowRainMuzzleEffect_.play( muzzlePos, player_->orient() );
			arrowRainEffect_.play( rainCenter, player_->orient() );
			debugBVView_.pushCircle( rainCenter, kArrowRainRadius, 1500ms, { 1.f, 0.f, 0.f, 1.f } );
			break;
		}
		case SwordEffect::RedEnergyExplosion: {
			auto explosionPos = player_->renderState().pos + player_->forward() * 6.5f;
			if ( terrain_ && !assetManager_.terrain()->heightField.empty() ) {
				const auto terrainPos = terrain_->renderState().pos;
				const float localX = explosionPos.x() - terrainPos.x();
				const float localZ = explosionPos.z() - terrainPos.z();
				const float groundY = terrainPos.y()
					+ assetManager_.terrain()->heightField.getHeightAt( localX, localZ );
				explosionPos = { explosionPos.x(), groundY + 0.1f, explosionPos.z() };
			}
			else {
				explosionPos += mu::Vec3( 0.f, 0.1f, 0.f );
			}
			redEnergyExplosionEffect_.play( explosionPos, player_->orient(), player_->forward() );
			break;
		}
		case SwordEffect::CrystalsCrossFade: {
			crystalsCrossFadeEffect_.play( slashPos );
			break;
		}
		case SwordEffect::EnergyExplosionArrow: {
			const auto origin = player_->renderState().pos
				+ mu::Vec3{ 0.f, 1.2f, 0.f }
				+ player_->forward() * 0.5f;
			energyExplosionArrowEffect_.play( origin, player_->orient() );
			break;
		}
		case SwordEffect::TornadoShot: {
			if ( tornadoShotActive_ )
				tornadoShotEffect_.stop();
			const auto origin = player_->renderState().pos
				+ mu::Vec3{ 0.f, 0.8f, 0.f }
				+ player_->forward() * 0.5f;
			tornadoMuzzleEffect_.play( origin );
			tornadoShotEffect_.play( origin, player_->orient() );
			tornadoShotActive_  = true;
			tornadoShotPos_     = origin;
			tornadoShotDir_     = player_->forward();
			tornadoShotOrient_  = player_->orient();
			tornadoShotElapsed_ = 0s;
			break;
		}
		}
	}

	// 마우스 민감도를 기반으로 플레이어 yaw, 카메라 pitch를 계산한다.
	// (pitch를 플레이어에 적용하게 되면, 플레이어가 고개를 들고 내리는 게 아니라 굴러버린다.)
    const auto mouseSensitivity = mu::pi * 2.f;

	const auto yaw = mu::Radian(mouseDeltaX_ * mouseSensitivity / static_cast<float>(gClientRect.right - gClientRect.left));
	auto yawRotation = mu::NQuat(mu::Radian(0.f), mu::Radian(0.f), yaw);

	cameraPitch_ = std::clamp(
		static_cast<float>(cameraPitch_) + mouseDeltaY_ * mouseSensitivity / static_cast<float>(gClientRect.bottom - gClientRect.top),
		-mu::pi * 0.16f,
		mu::pi * 0.3f
	);

	if (!playerDead_) {
		player_->setOrient(player_->orient() * yawRotation);
		camera_.setOffsetFromTargetPreRotation( mu::NQuat(mu::Radian(0.f), cameraPitch_, mu::Radian(0.f)) );
	}
	else {
		cameraYaw_ += yaw;
		camera_.setOffsetFromTargetPreRotation( mu::NQuat(mu::Radian(0.f), cameraPitch_, cameraYaw_) );
	}

	mouseDeltaX_ = 0;
	mouseDeltaY_ = 0;

	// Enter 키를 누르면 커서 캡처 플래그를 활성화/비활성화한다.
	if ( (keyboardStateCurr_[VK_RETURN] & 0x80) && !(keyboardStatePrev_[VK_RETURN] & 0x80) ) {
		cursorCaptureEnabled_ = !cursorCaptureEnabled_;
		if (cursorCaptureEnabled_) {
			captureCursor();
		}
		else {
			releaseCursor();
		}
	}

	// C key: toggle CSM cascade debug visualization
	if ( (keyboardStateCurr_['C'] & 0x80) && !(keyboardStatePrev_['C'] & 0x80) ) {
		gfx_.toggleCsmDebugVisualization();
	}

	// H key: toggle Hi-Z occlusion culling
	if ( (keyboardStateCurr_['H'] & 0x80) && !(keyboardStatePrev_['H'] & 0x80) ) {
		gfx_.setHiZCullEnabled(!gfx_.isHiZCullEnabled());
	}

	// G key: cycle GBuffer debug view (deferred path only)
	if ( (keyboardStateCurr_['G'] & 0x80) && !(keyboardStatePrev_['G'] & 0x80) ) {
		gfx_.cycleGBufferDebugMode();
	}

	// U key: toggle UI debug overlay (colored bounding boxes)
	if ( (keyboardStateCurr_['U'] & 0x80) && !(keyboardStatePrev_['U'] & 0x80) ) {
		uiManager_.toggleDebugMode();
	}

	// F key: emit particles for testing
	if ( (keyboardStateCurr_['F'] & 0x80) && !(keyboardStatePrev_['F'] & 0x80) ) {
		const auto slashPos = player_->renderState().pos
			+ player_->forward() * 1.f
			+ mu::Vec3( 0.f, 1.0f, 0.f );
		swordSlash1Effect_.play(slashPos);
	}

	// Space 키를 누르면 커서 보이기 플래그를 활성화/비활성화한다.
	if ( (keyboardStateCurr_[VK_SPACE] & 0x80) && !(keyboardStatePrev_[VK_SPACE] & 0x80) ) {
		cursorShowEnabled_ = !cursorShowEnabled_;
		if (cursorShowEnabled_) {
			showCursor();
		}
		else {
			hideCursor();
		}
	}

	// Z 키를 누르면 중력을 켜고 끈다 (임시 디버그 기능).
	if ( (keyboardStateCurr_['Z'] & 0x80) && !(keyboardStatePrev_['Z'] & 0x80) ) {
		gravityEnabled_ = !gravityEnabled_;
		physicsWorld_.setGravity(gravityEnabled_ ? mu::Vec3{ 0.f, -9.8f, 0.f } : mu::Vec3{ 0.f, 0.f, 0.f });
	}

	// --- Physics constraint / ragdoll debug tools ---
	// Keys 1-5: spawn test structures. K: clear all. R: impulse ray.
	// Comma/Period: halve/double impulse strength. M: slow motion cycle.
	// 1-8: spawn test object. V: OBB visualization. I: random blast. P: freeze toggle.
#define RD_KEY_DOWN(k) ((keyboardStateCurr_[k] & 0x80) && !(keyboardStatePrev_[k] & 0x80))
	if (RD_KEY_DOWN('1')) spawnTestObject(1);
	if (RD_KEY_DOWN('2')) spawnTestObject(2);
	if (RD_KEY_DOWN('3')) spawnTestObject(3);
	if (RD_KEY_DOWN('4')) spawnTestObject(4);
	if (RD_KEY_DOWN('5')) spawnTestObject(5);
	if (RD_KEY_DOWN('6')) spawnTestObject(6);
	if (RD_KEY_DOWN('7')) spawnTestObject(7);
	if (RD_KEY_DOWN('8')) spawnTestObject(8);
	if (RD_KEY_DOWN('K')) clearTestObjects();
	if (RD_KEY_DOWN('R')) fireImpulseRay();

	if (RD_KEY_DOWN(VK_OEM_COMMA))  rdImpulseStrength_ = std::max(0.5f,  rdImpulseStrength_ * 0.5f);
	if (RD_KEY_DOWN(VK_OEM_PERIOD)) rdImpulseStrength_ = std::min(500.f, rdImpulseStrength_ * 2.0f);

	if (RD_KEY_DOWN('M')) {
		if      (rdDebugTimeScale_ >= 1.f)   rdDebugTimeScale_ = 0.25f;
		else if (rdDebugTimeScale_ >= 0.25f) rdDebugTimeScale_ = 0.05f;
		else                                  rdDebugTimeScale_ = 1.0f;
	}
	if (RD_KEY_DOWN('V')) rdShowBodies_ = !rdShowBodies_;

	if (RD_KEY_DOWN('I')) {
		static std::mt19937 rng{ std::random_device{}() };
		std::uniform_real_distribution<float> d(-1.f, 1.f);
		for (auto& obj : rdObjects_) {
			for (auto& b : obj.bodies) {
				if (b->motionType() != MotionType::Dynamic) continue;
				// Bias upward component so blasts send bodies outward, not into the floor.
				const mu::Vec3 rawDir{ d(rng), std::abs(d(rng)) * 0.5f + 0.3f, d(rng) };
				const mu::Vec3 dir = mu::Vec3(mu::NVec3(rawDir));
				b->applyImpulse(dir * rdImpulseStrength_, b->pos());
			}
		}
	}

	if (RD_KEY_DOWN('P')) {
		rdFrozen_ = !rdFrozen_;
		if (rdFrozen_)
			for (auto& obj : rdObjects_) obj.freezeAll();
	}
#undef RD_KEY_DOWN
}

void Game::cullObjects() {
	auto entities = std::vector< std::shared_ptr<Object> >{};
	entities.reserve(1u + goblins_.size());

	entities.push_back(goblin_);
	for (auto& g : goblins_) {
		entities.push_back(g);
	}

	// Gribb-Hartmann: extract 6 frustum planes from view-proj matrix.
	// DirectXMath row-vector convention: v' = v * M, so P_clip[j] = dot(P, col(j)).
	// Plane equation: dot(plane.xyz, P) + plane.w >= 0 means inside.
	const auto vp = camera_.view() * camera_.proj();
	const auto c0 = vp.col(0);
	const auto c1 = vp.col(1);
	const auto c2 = vp.col(2);
	const auto c3 = vp.col(3);

	const mu::Vec4 planes[6] = {
		c3 + c0,   // left
		c3 - c0,   // right
		c3 + c1,   // bottom
		c3 - c1,   // top
		c2,        // near
		c3 - c2,   // far
	};

	for (auto& entt : entities) {
		auto& rootShape = entt->body().worldBVH().nodes[0].shape;
		bool culled = false;

		if (std::holds_alternative<AABB>(rootShape)) {
			const auto& aabb = std::get<AABB>(rootShape);
			const mu::Vec3 c = aabb.center;
			const mu::Vec3 h = aabb.size * 0.5f;

			for (const auto& pl : planes) {
				// Projection radius of AABB onto plane normal
				const float e = std::abs(pl.x()) * h.x()
							  + std::abs(pl.y()) * h.y()
							  + std::abs(pl.z()) * h.z();
				const float dist = pl.x() * c.x() + pl.y() * c.y()
								 + pl.z() * c.z() + pl.w();
				if (dist + e < 0.f) { culled = true; break; }
			}
		} else {
			const auto& obb = std::get<OBB>(rootShape);
			const mu::Vec3 c  = obb.center;
			const mu::Vec3 ax = obb.orient.rotate(mu::Vec3(1.f, 0.f, 0.f));
			const mu::Vec3 ay = obb.orient.rotate(mu::Vec3(0.f, 1.f, 0.f));
			const mu::Vec3 az = obb.orient.rotate(mu::Vec3(0.f, 0.f, 1.f));

			for (const auto& pl : planes) {
				const float e =
					std::abs(pl.x()*ax.x() + pl.y()*ax.y() + pl.z()*ax.z()) * obb.halfExtents.x()
				  + std::abs(pl.x()*ay.x() + pl.y()*ay.y() + pl.z()*ay.z()) * obb.halfExtents.y()
				  + std::abs(pl.x()*az.x() + pl.y()*az.y() + pl.z()*az.z()) * obb.halfExtents.z();
				const float dist = pl.x()*c.x() + pl.y()*c.y() + pl.z()*c.z() + pl.w();
				if (dist + e < 0.f) { culled = true; break; }
			}
		}

		entt->setFrustumCulled(culled);
	}
}

void Game::cullObjectsForShadow() {
	const u32t cascadeCount = dirLight_.cascadeCount();
	if (cascadeCount == 0u) return;

	auto entities = std::vector<std::shared_ptr<Object>>{};
	entities.reserve(1u + goblins_.size());
	entities.push_back(goblin_);
	for (auto& g : goblins_) entities.push_back(g);

	// Gribb-Hartmann planes for each cascade (row-vector convention: col(i))
	struct CascadePlanes { mu::Vec4 p[6]; };
	auto cascadePlanes = std::vector<CascadePlanes>(cascadeCount);
	for (u32t ci = 0u; ci < cascadeCount; ++ci) {
		const auto vp = dirLight_.cascadeViews()[ci] * dirLight_.cascadeProjs()[ci];
		const auto c0 = vp.col(0), c1 = vp.col(1), c2 = vp.col(2), c3 = vp.col(3);
		cascadePlanes[ci].p[0] = c3 + c0;
		cascadePlanes[ci].p[1] = c3 - c0;
		cascadePlanes[ci].p[2] = c3 + c1;
		cascadePlanes[ci].p[3] = c3 - c1;
		cascadePlanes[ci].p[4] = c2;
		cascadePlanes[ci].p[5] = c3 - c2;
	}

	for (auto& entt : entities) {
		auto& rootShape = entt->body().worldBVH().nodes[0].shape;

		// Visible in any cascade => not shadow-culled
		bool visibleInAny = false;
		for (u32t ci = 0u; ci < cascadeCount && !visibleInAny; ++ci) {
			const auto* pl = cascadePlanes[ci].p;
			bool inside = true;

			if (std::holds_alternative<AABB>(rootShape)) {
				const auto& aabb = std::get<AABB>(rootShape);
				const mu::Vec3 c = aabb.center;
				const mu::Vec3 h = aabb.size * 0.5f;
				for (int i = 0; i < 6 && inside; ++i) {
					const float e = std::abs(pl[i].x()) * h.x()
					              + std::abs(pl[i].y()) * h.y()
					              + std::abs(pl[i].z()) * h.z();
					const float dist = pl[i].x()*c.x() + pl[i].y()*c.y() + pl[i].z()*c.z() + pl[i].w();
					if (dist + e < 0.f) inside = false;
				}
			} else {
				const auto& obb = std::get<OBB>(rootShape);
				const mu::Vec3 c  = obb.center;
				const mu::Vec3 ax = obb.orient.rotate(mu::Vec3(1.f, 0.f, 0.f));
				const mu::Vec3 ay = obb.orient.rotate(mu::Vec3(0.f, 1.f, 0.f));
				const mu::Vec3 az = obb.orient.rotate(mu::Vec3(0.f, 0.f, 1.f));
				for (int i = 0; i < 6 && inside; ++i) {
					const float e =
						std::abs(pl[i].x()*ax.x() + pl[i].y()*ax.y() + pl[i].z()*ax.z()) * obb.halfExtents.x()
					  + std::abs(pl[i].x()*ay.x() + pl[i].y()*ay.y() + pl[i].z()*ay.z()) * obb.halfExtents.y()
					  + std::abs(pl[i].x()*az.x() + pl[i].y()*az.y() + pl[i].z()*az.z()) * obb.halfExtents.z();
					const float dist = pl[i].x()*c.x() + pl[i].y()*c.y() + pl[i].z()*c.z() + pl[i].w();
					if (dist + e < 0.f) inside = false;
				}
			}

			if (inside) visibleInAny = true;
		}

		entt->setShadowCulled(!visibleInAny);
	}
}

void Game::applyHiZCulling() {
	if (!gfx_.isHiZCullEnabled()) {
		for (auto& entt : { std::static_pointer_cast<Object>(goblin_) }) {
			entt->setHiZCulled(false);
			if (auto* blender = entt->animBlender())
				blender->setCulled(entt->isFrustumCulled());
		}
		for (auto& g : goblins_) {
			g->setHiZCulled(false);
			if (auto* blender = g->animBlender())
				blender->setCulled(g->isFrustumCulled());
		}
		return;
	}

	auto applyToEntity = [&](const std::shared_ptr<Object>& entt) {
		const bool hiZVisible = gfx_.getHiZObjectVisible(entt->renderObjectId());
		entt->setHiZCulled(!hiZVisible);
		if (auto* blender = entt->animBlender())
			blender->setCulled(entt->isFrustumCulled() || !hiZVisible);
	};
	applyToEntity(goblin_);
	for (auto& g : goblins_)
		applyToEntity(g);
}

// 커서가 클라이언트 영역 바깥으로 나가지 못하도록 한다.
// 한번 설정해놓으면, releaseCursor를 호출하기 전까지 커서는 계속 클라이언트 영역에 갇혀있는다.
void Game::captureCursor() {
    auto ul = POINT{ gClientRect.left, gClientRect.top };
    auto lr = POINT{ gClientRect.right, gClientRect.bottom };

    // 클라이언트의 외곽 좌표를 윈도우 좌표로 변환
    MapWindowPoints(ghWnd, nullptr, &ul, 1);
    MapWindowPoints(ghWnd, nullptr, &lr, 1);

    auto clipRect = RECT{ ul.x, ul.y, lr.x, lr.y };

    ClipCursor(&clipRect);
}

// 커서 캡처가 설정되어있다면 해제한다.
// captureCursor로 활성화된 커서 캡처를 해제하는 역할을 한다.
void Game::releaseCursor() {
	ClipCursor(nullptr);
}

void Game::hideCursor() {
	// ShowCursor()는 내부적으로 display counter를 증가/감소시키는 구조라서
	// 반복 호출해 정확히 숨기거나 표시해야 한다.
	while (ShowCursor(false) >= 0) {}
}

void Game::showCursor() {
	// ShowCursor()는 내부적으로 display counter를 증가/감소시키는 구조라서
	// 반복 호출해 정확히 숨기거나 표시해야 한다.
	while (ShowCursor(true) < 0) {}
}

}	// namespace StandAlone
