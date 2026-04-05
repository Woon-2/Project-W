#ifndef room_server_asset_manager_hpp
#define room_server_asset_manager_hpp

#include "Model.hpp"
#include "Level.hpp"

class AssetManager {
public:
	AssetManager() = default;

	void loadAssets();

	const Model* modelCube() const { return &modelCube_; }
	const Model* modelPlayer() const { return &modelPlayer_; }
	const Model* modelGoblin() const { return &modelGoblin_; }

	const Level* level() const { return &level_; }

private:
	Model modelCube_;
	Model modelPlayer_;
	Model modelGoblin_;
	
	Level level_;
};

#endif // room_server_asset_manager_hpp