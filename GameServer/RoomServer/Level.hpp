#ifndef room_server_level_hpp
#define room_server_level_hpp

#include "object.hpp"
#include "goblin.hpp"
#include "terrain.hpp"

class AssetManager;

struct GoblinSpawnerInfo {
	mu::Vec3 center;
	float    activityRadius;
	int32    startIdx;
	int32    count;
};

struct Level {
	std::vector<Cube>              cubes;
	std::vector<Player>            playerStarts;
	std::vector<Goblin>            goblins;
	std::vector<GoblinSpawnerInfo> goblinSpawners;

	// Shared, read-only terrain chunk height fields. Loaded once at boot; the
	// single Level instance is shared across all rooms (see RoomManager), so
	// this height field data is shared too (no per-room copy).
	TerrainChunkManager            terrainChunks;
};

Level loadLevelFromFile(const std::filesystem::path& path, const AssetManager& assetManager);

#endif // room_server_level_hpp