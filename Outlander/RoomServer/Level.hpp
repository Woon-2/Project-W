#ifndef room_server_level_hpp
#define room_server_level_hpp

#include "object.hpp"
#include "goblin.hpp"
#include "terrain.hpp"
#include <vector>

class AssetManager;

struct Level {
	std::vector<Cube>   cubes;
	std::vector<Player> playerStarts;

	// Shared, read-only terrain chunk height fields + stronghold definitions
	// (both parsed from chunks_index.bin). Loaded once at boot; the single Level
	// instance is shared across all rooms (see RoomManager).
	TerrainChunkManager terrainChunks;

	// Back-reference to the owning AssetManager (set at load). Room reaches model
	// and animation assets through this to build monster pools and strongholds.
	// Monsters are no longer authored as level nodes; strongholds
	// (terrainChunks.strongholds()) drive spawning.
	const AssetManager* assetManager = nullptr;
};

Level loadLevelFromFile(const std::filesystem::path& path, const AssetManager& assetManager);

#endif // room_server_level_hpp