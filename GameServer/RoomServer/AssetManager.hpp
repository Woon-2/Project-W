#ifndef room_server_asset_manager_hpp
#define room_server_asset_manager_hpp

#include "Model.hpp"
#include "Level.hpp"
#include "serverAnimation.hpp"
#include "skill/skillTypes.hpp"

class AssetManager {
public:
	AssetManager() = default;

	void loadAssets();

	const Model* modelCube()   const { return &modelCube_; }
	const Model* modelPlayer() const { return &modelPlayer_; }
	const Model* modelGoblin() const { return &modelGoblin_; }

	const Level* level() const { return &level_; }

	const std::vector<ServerAnimClip>& playerAnimations() const { return playerAnimations_; }
	const std::vector<ServerAnimClip>& goblinAnimations() const { return goblinAnimations_; }

	// 부팅 시 1회 컴파일되어 전 룸이 공유하는 읽기 전용 스킬 레지스트리.
	const std::vector<SkillAsset>& skillAssets() const { return skillAssets_; }

	static const ServerAnimClip* findClip(const std::vector<ServerAnimClip>& set,
	                                      std::string_view name);

private:
	Model modelCube_;
	Model modelPlayer_;
	Model modelGoblin_;

	Level level_;

	std::vector<ServerAnimClip> playerAnimations_;
	std::vector<ServerAnimClip> goblinAnimations_;

	std::vector<SkillAsset> skillAssets_;   // 전 룸 공유(부팅 1회 컴파일, 읽기 전용)
};

#endif // room_server_asset_manager_hpp