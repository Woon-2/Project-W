#ifndef room_server_level_hpp
#define room_server_level_hpp

#include "object.hpp"
#include "goblin.hpp"

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
	TerrainObject                  terrain;
	std::vector<GoblinSpawnerInfo> goblinSpawners;
};

Level loadLevelFromFile(const std::filesystem::path& path, const AssetManager& assetManager);

#endif // room_server_level_hpp