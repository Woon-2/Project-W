#include "rspch.hpp"
#include "Level.hpp"
#include "binaryImport.hpp"
#include "assetManager.hpp"

void importCube(std::ifstream& ifs, const AssetManager& assetManager, Object& cube) {
	cube.setModel(assetManager.modelCube());

	const auto meshName = readText(ifs, "Mesh");
	const auto materialSetName = readText(ifs, "MaterialSet");
	const auto materialSetIdx = readInteger(ifs, "MaterialSetIndex");

	cube.setMaterialSetIdx(materialSetIdx);
}

void importGoblinSpawner(std::ifstream& ifs, const AssetManager& assetManager, Goblin& goblin) {
	goblin.setModel(assetManager.modelGoblin());
}

void importTerrain(std::ifstream& ifs, TerrainObject& terrain) {
	const auto manifestPath = readText(ifs, "ManifestPath");
	const auto terrainDir   = std::filesystem::path(manifestPath).parent_path();
	terrain.heightField()   = loadTerrainHeightFieldFromFiles(terrainDir);
}

void importNode(std::ifstream& ifs, const AssetManager& assetManager, Level& level) {
	readHeadTag(ifs, "Node");
	const auto type = readText(ifs, "Type");
	const auto name = readText(ifs, "Name");

	gSharedLog << "[Level Load] 레벨 노드 " << name << " 로드 완료\n";
	dumpLog();

	readHeadTag(ifs, "LocalTRS");
	const auto localT = readVec3(ifs, "Position");
	const auto localR = readVec4(ifs, "Rotation");
	const auto localS = readVec3(ifs, "Scale");
	readTailTag(ifs, "LocalTRS");

	readHeadTag(ifs, "WorldTRS");
	const auto worldT = readVec3(ifs, "Position");
	const auto worldR = readVec4(ifs, "Rotation");
	const auto worldS = readVec3(ifs, "Scale");
	readTailTag(ifs, "WorldTRS");

	Object object{};
	object.setPos(DirectX::XMLoadFloat3(&worldT));
	object.setOrient(DirectX::XMLoadFloat4(&worldR));
	object.setScale(DirectX::XMLoadFloat3(&worldS));

	if (type == "Cube") {
		auto& cube = level.cubes.emplace_back(std::move(object));
		importCube(ifs, assetManager, cube);
	}
	else if (type == "PlayerStart") {
		level.playerStarts.push_back(std::move(object));
	}
	else if (type == "Terrain") {
		level.terrain = TerrainObject(std::move(object));
		importTerrain(ifs, level.terrain);
	}
	else if (type == "GoblinSpawner") {
		static constexpr float kActivityRadius = 40.f;
		static constexpr float kSpawnRadius    = 15.f;
		static constexpr int32 kCount          = 5;
		static constexpr float kMinDist        = 2.f;

		std::mt19937 rng{ std::random_device{}() };
		std::uniform_real_distribution<float> distR    (0.f, 1.f);
		std::uniform_real_distribution<float> distAngle(0.f, 2.f * DirectX::XM_PI);

		const mu::Vec3 center   = object.pos();
		const float    baseY    = center.y();
		const int32    startIdx = static_cast<int32>(level.goblins.size());

		std::vector<mu::Vec3> placed;
		placed.reserve(kCount);

		for (int32 i = 0; i < kCount; ++i) {
			mu::Vec3 spawnPos;
			int32 attempts = 0;
			do {
				float r     = kSpawnRadius * std::sqrt(distR(rng));
				float theta = distAngle(rng);
				spawnPos    = mu::Vec3(center.x() + r * std::cosf(theta),
				                      baseY,
				                      center.z() + r * std::sinf(theta));
				++attempts;
			} while (attempts < 100 &&
			         std::any_of(placed.begin(), placed.end(), [&](const mu::Vec3& p) {
			             return (spawnPos - p).len2() < kMinDist * kMinDist;
			         }));

			placed.push_back(spawnPos);
			Object copy = object;
			copy.setPos(spawnPos);
			auto& g = level.goblins.emplace_back(Goblin(std::move(copy)));
			importGoblinSpawner(ifs, assetManager, g);
		}

		level.goblinSpawners.push_back({ center, kActivityRadius, startIdx, kCount });
	}
	else {
		// no-op
	}

	const auto childCnt = readInteger(ifs, "ChildCnt");
	readHeadTag(ifs, "Children");
	for (int i = 0; i < childCnt; ++i) {
		importNode(ifs, assetManager, level);
	}
	readTailTag(ifs, "Children");

	readTailTag(ifs, "Node");
}

Level loadLevelFromFile(const std::filesystem::path& path, const AssetManager& assetManager) {
	Level ret{};

	auto ifs = std::ifstream(path, std::ios::binary);
	DISPLAY_ERROR_STR(ifs.good(), "[File I/O Error]: loadModelFromFile: "s + path.string() + " 파일을 열 수 없습니다."s, true);

	readHeadTag(ifs, "Level");
	const auto nodeCnt = readInteger(ifs, "NodeCnt");

	importNode(ifs, assetManager, ret);

	readTailTag(ifs, "Level");

	gSharedLog << "[Level Load] File I/O: 레벨 " << path << "로드 완료\n";
	return ret;
}
