#include "player.hpp"

#include "inputSystem.hpp"
#include "physicsSystem.hpp"

#include "d3d12engine/d3d12Engine.hpp"
#include "assetMap.hpp"

void Player::init(gfx::d3d12engine::Core& core) {
	createComponent<RigidBody>();
	createComponent<PlayerController>();
	createComponent<gfx::d3d12engine::Coord>();
	createComponent<gfx::d3d12engine::Model>( assetModelInfo(AssetModel::Helicopter).id,
		core, as<gfx::d3d12engine::Coord>()
	);
}

void MU_CALLCONV Player::addCamera( mu::Vec3 offset, float timeLag,
	const gfx::d3d12::Camera::Config& config
) {
	createComponent<gfx::d3d12engine::Camera>(config);
	as<gfx::d3d12engine::Camera>().setOffset(offset);
	as<gfx::d3d12engine::Camera>().setTimeLag(timeLag);
	as<gfx::d3d12engine::Camera>().attach( as<gfx::d3d12engine::Model>() );
}

void Player::update(float deltaTime) {
	as<gfx::d3d12engine::Coord>().get()
		<< mu::translate( as<RigidBody>().deltaPosition() );
	as<gfx::d3d12engine::Camera>().update(deltaTime);
}