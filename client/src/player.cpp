#include "player.hpp"

#include "inputSystem.hpp"
#include "physicsSystem.hpp"

#include "d3d12engine/d3d12Engine.hpp"
#include "assetMap.hpp"

void Player::init(gfx::d3d12engine::Core& core) {
	createComponent<RigidBody>();
	createComponent<PlayerController>();
	createComponent<gfx::d3d12engine::Coord>();
	createComponent<gfx::d3d12engine::Model>( assetModelID(AssetModel::Helicopter),
		core, as<gfx::d3d12engine::Coord>()
	);
}

void Player::update() {
	as<gfx::d3d12engine::Coord>().get()
		<< mu::translate( as<RigidBody>().deltaPosition() );
}