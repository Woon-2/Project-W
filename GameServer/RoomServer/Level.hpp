#ifndef room_server_level_hpp
#define room_server_level_hpp

#include "object.hpp"

class AssetManager;

struct Level {
	std::vector<Object> cubes;
	std::vector<Object> playerStarts;
};

Level loadLevelFromFile(const std::filesystem::path& path, const AssetManager& assetManager);

#endif // room_server_level_hpp