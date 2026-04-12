#ifndef room_server_level_hpp
#define room_server_level_hpp

#include "object.hpp"

class AssetManager;

struct Level {
	std::vector<Cube>   cubes;
	std::vector<Player> playerStarts;
	std::vector<Goblin> goblins;
	TerrainObject       terrain;
};

Level loadLevelFromFile(const std::filesystem::path& path, const AssetManager& assetManager);

#endif // room_server_level_hpp