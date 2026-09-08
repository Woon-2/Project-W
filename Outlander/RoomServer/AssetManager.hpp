#ifndef room_server_asset_manager_hpp
#define room_server_asset_manager_hpp

#include "Model.hpp"
#include "Level.hpp"
#include "serverAnimation.hpp"
#include "skill/skillTypes.hpp"
#include "skill/skillLoadout.hpp"
#include "chargeConfig.hpp"
#include "../common/inventory.hpp"

class AssetManager {
public:
	AssetManager() = default;

	void loadAssets();

	const Model* modelCube()      const { return &modelCube_; }
	const Model* modelStronghold()      const { return &modelStronghold_; }
	const Model* modelPlayer()    const { return &modelPlayer_; }
	const Model* modelGoblin()    const { return &modelGoblin_; }
	const Model* modelHobgoblin() const { return &modelHobgoblin_; }
	const Model* modelSnake()     const { return &modelSnake_; }
	const Model* modelMushroom()  const { return &modelMushroom_; }
	const Model* modelBomber()    const { return &modelBomber_; }
	const Model* modelBirdy()     const { return &modelBirdy_; }
	const Model* modelSlime()     const { return &modelSlime_; }
	const Model* modelTreant()    const { return &modelTreant_; }
	// Named variants: share their base monster's animations, only the model differs.
	const Model* modelGrandbaum() const { return &modelGrandbaum_; }   // Treant variant
	const Model* modelIsys()      const { return &modelIsys_; }        // Birdy variant
	const Model* modelBoss()      const { return &modelBoss_; }        // final boss (own 14-clip rig)

	const Level* level() const { return &level_; }

	const std::vector<ServerAnimClip>& playerAnimations()   const { return playerAnimations_; }
	const std::vector<ServerAnimClip>& goblinAnimations()   const { return goblinAnimations_; }
	const std::vector<ServerAnimClip>& snakeAnimations()    const { return snakeAnimations_; }
	const std::vector<ServerAnimClip>& mushroomAnimations() const { return mushroomAnimations_; }
	const std::vector<ServerAnimClip>& bomberAnimations()   const { return bomberAnimations_; }
	const std::vector<ServerAnimClip>& birdyAnimations()    const { return birdyAnimations_; }
	const std::vector<ServerAnimClip>& slimeAnimations()    const { return slimeAnimations_; }
	const std::vector<ServerAnimClip>& treantAnimations()   const { return treantAnimations_; }
	const std::vector<ServerAnimClip>& bossAnimations()     const { return bossAnimations_; }

	// 부팅 시 1회 컴파일되어 전 룸이 공유하는 읽기 전용 스킬 레지스트리.
	const std::vector<SkillAsset>& skillAssets() const { return skillAssets_; }

	// 부팅 시 1회 로드되는 스택 충전 경제 튜닝(전 룸 공유, 읽기 전용).
	const ChargeConfig& chargeConfig() const { return chargeConfig_; }

	// 무기별 스킬 로드아웃(기본공격 + 3슬롯 + 코스트). 부팅 시 1회 빌드.
	const SkillLoadout& loadout() const { return loadout_; }
	const ItemCatalog& itemCatalog() const { return itemCatalog_; }

	static const ServerAnimClip* findClip(const std::vector<ServerAnimClip>& set,
	                                      std::string_view name);

private:
	Model modelCube_;
	Model modelStronghold_;
	Model modelPlayer_;
	Model modelGoblin_;
	Model modelHobgoblin_;
	Model modelSnake_;
	Model modelMushroom_;
	Model modelBomber_;
	Model modelBirdy_;
	Model modelSlime_;
	Model modelTreant_;
	Model modelGrandbaum_;   // named Treant variant (Treant anims)
	Model modelIsys_;        // named Birdy variant  (Birdy anims)
	Model modelBoss_;        // final boss (own 14-clip rig)

	Level level_;

	std::vector<ServerAnimClip> playerAnimations_;
	std::vector<ServerAnimClip> goblinAnimations_;
	std::vector<ServerAnimClip> snakeAnimations_;
	std::vector<ServerAnimClip> mushroomAnimations_;
	std::vector<ServerAnimClip> bomberAnimations_;
	std::vector<ServerAnimClip> birdyAnimations_;
	std::vector<ServerAnimClip> slimeAnimations_;
	std::vector<ServerAnimClip> treantAnimations_;
	std::vector<ServerAnimClip> bossAnimations_;

	std::vector<SkillAsset> skillAssets_;   // 전 룸 공유(부팅 1회 컴파일, 읽기 전용)
	ChargeConfig            chargeConfig_;  // 전 룸 공유(부팅 1회 로드, 읽기 전용)
	SkillLoadout            loadout_;       // 무기→스킬 로드아웃(부팅 1회 빌드)
	ItemCatalog             itemCatalog_;   // shared item definitions + starter inventory
};

#endif // room_server_asset_manager_hpp
