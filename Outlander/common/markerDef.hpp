#ifndef common_marker_def_hpp
#define common_marker_def_hpp

#include "mathUtil.hpp"
#include <string>

// A generic placement marker authored in the scene and baked into
// chunks_index.bin (the "Marker" section). Carries only a type tag, a name, and
// a world transform -- intentionally lighter than Zone/Stronghold. Gameplay code
// reads these by type/name to place things like boss spawn points or barriers.
// Shared by RoomServer and the client; both parse the same section.
struct MarkerDef {
    std::string type;   // free-form category, e.g. "BossSpawn", "Wall"
    std::string name;   // instance name (defaults to the GameObject name)
    mu::Vec3    pos{};
    mu::NQuat   orient{};
    mu::Vec3    scale{ 1.f, 1.f, 1.f };
};

#endif // common_marker_def_hpp
