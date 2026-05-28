#ifndef room_server_asset_manager_hpp
#define room_server_asset_manager_hpp

#include "Model.hpp"
#include "Level.hpp"
#include "serverAnimation.hpp"

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

	static const ServerAnimClip* findClip(const std::vector<ServerAnimClip>& set,
	                                      std::string_view name);

private:
	Model modelCube_;
	Model modelPlayer_;
	Model modelGoblin_;

	Level level_;

	std::vector<ServerAnimClip> playerAnimations_;
	std::vector<ServerAnimClip> goblinAnimations_;
};

#endif // room_server_asset_manager_hpp