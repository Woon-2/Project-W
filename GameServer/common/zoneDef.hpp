#ifndef common_zone_def_hpp
#define common_zone_def_hpp

#include "mathUtil.hpp"
#include <cstdint>
#include <string>
#include <vector>

// Plain data describing a trigger Zone authored in chunks_index.bin.
// Shared by RoomServer (authoritative gameplay zones) and the client
// (local cosmetic zones). Intentionally free of any engine/Object dependency:
// the target factions are stored as a raw bitmask (OR of factionBit(faction))
// so this header does not need to know about the per-project Faction enum.

enum class ZoneShape : std::uint8_t {
    Box    = 0,   // oriented box: center + orient + halfExtents
    Sphere = 1,   // center + radius
};

// One primitive volume. A Zone is the union of its volumes.
struct ZoneVolumeDef {
    ZoneShape shape = ZoneShape::Box;
    mu::Vec3  center{};
    mu::NQuat orient{};        // box only (ignored for spheres)
    mu::Vec3  halfExtents{};   // box only
    float     radius = 0.f;    // sphere only
};

struct ZoneDef {
    int           id = 0;
    std::string   tag;                 // handler binding key (e.g. "boss_arena_1")
    std::uint32_t factionMask = 0;     // OR of factionBit(faction); which factions trigger this zone
    std::vector<ZoneVolumeDef> volumes;
};

#endif // common_zone_def_hpp
