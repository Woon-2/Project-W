#ifndef room_server_stronghold_def_hpp
#define room_server_stronghold_def_hpp

#include <vector>

// Plain data describing a stronghold authored in chunks_index.bin.
// Intentionally free of any engine/Object dependency so it can live inside
// ChunkIndex (terrain.hpp) and be passed around as pure configuration.
// ObjectType / Seconds / mu::* come from the precompiled header (sepch.hpp).

// Per monster-type population target within a stronghold.
struct PopulationDef {
    ObjectType type        = ObjectType::Goblin;
    int        targetCount = 0;            // desired alive count (= pool size for this type)
    int        maxPerWave  = 1;            // max revivals per respawn interval
    Seconds    respawnInterval{ 5.f };
};

struct StrongholdDef {
    int       id = 0;
    mu::Vec3  center{};
    mu::NQuat orient{};
    mu::Vec3  scale{ 1.f, 1.f, 1.f };
    float     activityRadius = 40.f;
    float     spawnRadius    = 15.f;
    int       maxHp          = 1000;
    Seconds   respawnDelay{ 30.f };        // rebuild delay after the stronghold is destroyed
    std::vector<PopulationDef> populations;
};

#endif // room_server_stronghold_def_hpp
