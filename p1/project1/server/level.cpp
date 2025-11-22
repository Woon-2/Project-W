#include "pch.hpp"
#include "level.hpp"
#include "binaryImport.hpp"
#include "assetManager.hpp"

void importCube(std::ifstream& ifs, const AssetManager& assetManager, Object& cube) {
	cube.setModel(assetManager.modelCube());

	const auto meshName = readText(ifs, "Mesh");
	const auto materialSetName = readText(ifs, "MaterialSet");
	const auto materialSetIdx = readInteger(ifs, "MaterialSetIndex");

	cube.setMaterialSetIdx(materialSetIdx);
}

void importNode(std::ifstream& ifs, const AssetManager& assetManager, Level& level) {
	readHeadTag(ifs, "Node");
	const auto type = readText(ifs, "Type");
	const auto name = readText(ifs, "Name");

	gSharedLog << "[Level Load] 레벨 노드 " << name << " 로드 완료\n";

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

	auto ifs = std::ifstream(path);
	DISPLAY_ERROR_STR(ifs.good(), "[File I/O Error]: loadModelFromFile: "s + path.string() + " 파일을 열 수 없습니다."s, true);

	readHeadTag(ifs, "Level");
	const auto nodeCnt = readInteger(ifs, "NodeCnt");

	importNode(ifs, assetManager, ret);

	readTailTag(ifs, "Level");

	gSharedLog << "[Level Load] File I/O: 레벨 " << path << "로드 완료\n";
	return ret;
}