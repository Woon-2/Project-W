// Offline self-test for common/particleGameplay.hpp (server-side path).
// Loads an effect JSON, imports the gameplay config and evaluates the
// deterministic sampler at a few timestamps. Not part of the solution build;
// compile ad hoc:
//   cl /std:c++latest /EHsc /I common tools\pgSelfTest.cpp common\simpleJson.cpp
#include "particleGameplay.hpp"
#include <iostream>

static void dump(const pg::GameplayConfig& g) {
    std::cout << "  bursts=" << g.bursts.size();
    if (!g.bursts.empty()) {
        const auto& b = g.bursts[0];
        std::cout << " [time=" << b.time << " count=" << b.countMin << ".." << b.countMax
                  << " cycles=" << b.cycleCount << " interval=" << b.repeatInterval
                  << " prob=" << b.probability << "]";
    }
    std::cout << "\n  emitRate=" << g.emitRate
              << " arcMode=" << g.arcMode
              << " arcDeg=" << g.arc * 57.2958f
              << " coneAngleDeg=" << g.coneAngle * 57.2958f
              << " coneRadius=" << g.coneRadius
              << "\n  speed=" << g.speedMin << ".." << g.speedMax
              << " life=" << g.lifetimeMin << ".." << g.lifetimeMax
              << " duration=" << g.duration
              << " looping=" << g.looping
              << " delay=" << g.startDelay
              << " simSpeed=" << g.simulationSpeed
              << "\n  sol=" << g.solEnabled << " begin=" << g.solSizeBegin << " end=" << g.solSizeEnd
              << " size=" << g.startSizeMin << ".." << g.startSizeMax
              << " unsupported='" << g.unsupportedSummary() << "'\n";
}

int main(int argc, char** argv) {
    const char* jsonPath = (argc > 1) ? argv[1]
        : "resources/effects/Crystals front attack_ParticleSystems.json";
    const char* sysName  = (argc > 2) ? argv[2] : "Crystals front attack";

    pg::GameplayConfig g;
    std::string err;
    if (!pg::importGameplayConfig(std::filesystem::path(jsonPath), sysName, g, &err)) {
        std::cout << "IMPORT FAIL: " << err << "\n";
        return 1;
    }
    std::cout << "IMPORT OK: " << sysName << "\n";
    dump(g);
    g.looping = false;  // crystals lua override

    pg::EmitterFrame frame{};   // identity at origin
    pg::GroundQuery  ground{};  // no terrain
    std::vector<pg::ParticleState> out;

    for (float t : { 0.02f, 0.10f, 0.20f, 0.31f, 0.40f }) {
        pg::evaluateParticles(g, 12345u, pg::PlayMode::Continuous, frame, t,
                              ground, pg::GroundConform::None, 1024, out);
        std::cout << "t=" << t << "s alive=" << out.size() << "\n";
        for (const auto& p : out) {
            std::cout << "   pos=(" << p.pos.x() << ", " << p.pos.y() << ", " << p.pos.z()
                      << ") sizeNow=" << p.sizeNow << " ageT=" << p.ageT << "\n";
        }
    }

    // -- override-built config (mirrors one piercing_multi.lua stab system) --
    // Expect: nothing before the wave time (0.12), then one particle moving
    // +Z at 25 u/s from the box offset, dead after 0.42s of life.
    {
        pg::GameplayConfig pm;  // defaults only; no JSON (name = "")
        pg::VfxSystemOverrides o;
        o.hasLooping = true;       o.looping = false;
        o.hasDuration = true;      o.duration = 0.64f;
        o.hasLifetime = true;      o.lifetimeMin = o.lifetimeMax = 0.42f;
        o.hasSpeed = true;         o.speedMin = o.speedMax = 0.f;
        o.hasMaxParticles = true;  o.maxParticles = 4;
        o.hasShapeType = true;     o.shapeType = pg::ShapeType::Box;
        o.hasShapePosition = true; o.shapePosition = { -0.62f, -1.08f, 0.f };
        o.hasBoxSize = true;       o.boxSize = { 0.56f, 0.42f, 0.f };
        o.hasVolLinear = true;     o.volLinear = { 0.f, 0.f, 25.f };
        o.hasMeshEulerDeg = true;  o.meshEulerDeg = { 0.f, -90.f, 0.f };
        o.hasBursts = true;        o.bursts = { pg::Burst{ 0.12f, 1, 1, 1, 0.01f, 1.f } };
        pg::applyOverrides(pm, o);

        std::cout << "\npiercing-multi style override system (wave time 0.12):\n";
        for (float t : { 0.05f, 0.15f, 0.40f, 0.60f }) {
            pg::evaluateParticles(pm, 777u, pg::PlayMode::Continuous, frame, t,
                                  ground, pg::GroundConform::None, 64, out);
            std::cout << "t=" << t << "s alive=" << out.size();
            for (const auto& p : out)
                std::cout << "  pos=(" << p.pos.x() << ", " << p.pos.y()
                          << ", " << p.pos.z() << ")";
            std::cout << "\n";
        }
    }

    // -- sub-emitter death chain (mirrors energy_explosion_arrow.lua) --------
    // Charge (root Emit, life 1.6) -Death-> Arrow (life 0.6, +Z 40)
    // -Death-> Hit (life 1.6, speed 0). Expect: Hit alive in (2.2, 3.8) at z=24.
    {
        auto makeCfg = [](float life, float speed, float size,
                          bool withBurst) {
            pg::GameplayConfig c;
            pg::VfxSystemOverrides o;
            o.hasLooping = true;   o.looping = false;
            o.hasLifetime = true;  o.lifetimeMin = o.lifetimeMax = life;
            o.hasSpeed = true;     o.speedMin = o.speedMax = speed;
            o.hasStartSize = true; o.startSizeMin = o.startSizeMax = size;
            o.hasShapeType = true; o.shapeType = pg::ShapeType::Point;
            o.hasDirection = true; o.direction = { 0.f, 0.f, 1.f };
            if (withBurst) {
                o.hasBursts = true;
                o.bursts = { pg::Burst{ 0.f, 1, 1, 1, 0.01f, 1.f } };
            }
            pg::applyOverrides(c, o);
            return c;
        };
        const pg::GameplayConfig charge = makeCfg(1.6f, 0.f,  8.f,  false);
        const pg::GameplayConfig arrow  = makeCfg(0.6f, 40.f, 0.3f, true);
        const pg::GameplayConfig hit    = makeCfg(1.6f, 0.f,  10.f, true);

        auto resolver = [&](int idx) -> pg::SystemRef {
            pg::SystemRef r;
            switch (idx) {
            case 0: r.cfg = &charge; r.seed = 1; r.mode = pg::PlayMode::Emit; break;
            case 1: r.cfg = &arrow;  r.seed = 2; r.chainParent = 0; break;
            case 2: r.cfg = &hit;    r.seed = 3; r.chainParent = 1; break;
            default: break;
            }
            return r;
        };

        std::cout << "\nEEA-style death chain (system 2 = Hit):\n";
        for (float t : { 2.1f, 2.3f, 3.7f, 3.9f }) {
            pg::evaluateSystemParticles(resolver, 2, frame, t, ground,
                                        pg::GroundConform::None, 64, out);
            std::cout << "t=" << t << "s alive=" << out.size();
            for (const auto& p : out)
                std::cout << "  pos=(" << p.pos.x() << ", " << p.pos.y()
                          << ", " << p.pos.z() << ") size=" << p.sizeNow;
            std::cout << "\n";
        }
    }
    return 0;
}
