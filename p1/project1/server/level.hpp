#ifndef __level_HPP
#define __level_HPP

#include "object.hpp"

class Level {
public:
	Level(const std::filesystem::path& path) : cubes( ), playerStarts ( ) {
		loadFromFile(path);
	}

	void loadFromFile(const std::filesystem::path& path);

	std::vector<Object> cubes;
	std::vector<Object> playerStarts;

private:
	void importNode(std::ifstream& ifs);
	void importCube(std::ifstream& ifs, Object& cube);
};

#endif	// __level_HPP