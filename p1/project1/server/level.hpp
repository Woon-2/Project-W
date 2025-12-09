#ifndef __level_HPP
#define __level_HPP

#include "object.hpp"

class AssetManager;

struct Level {
	std::vector<Object> cubes;
	std::vector<Object> playerStarts;
};

Level loadLevelFromFile(const std::filesystem::path& path, const AssetManager& assetManager);

#endif	// __level_HPP