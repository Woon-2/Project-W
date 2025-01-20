#include "player.hpp"

#include "inputSystem.hpp"
#include "physicsSystem.hpp"

#include "d3d12engine/d3d12Engine.hpp"
#include "assetMap.hpp"

Player::Player()
	: coordRot_() {}

void Player::init(gfx::d3d12engine::Core& core) {
	createComponent<RigidBody>();
	createComponent<PlayerController>();
	createComponent<gfx::d3d12engine::Coord>();
	createComponent<gfx::d3d12engine::Model>( assetModelInfo(AssetModel::Helicopter).id,
		core, as<gfx::d3d12engine::Coord>()
	);
	as<gfx::d3d12engine::Model>().get().markRenderPass(
		gfx::d3d12::rp::PBRIllumination::id
	);
	as<gfx::d3d12engine::Model>().get().markRenderPass(
		gfx::d3d12::rp::ShadowMap::id
	);
	as<gfx::d3d12engine::Model>().get().root()->coord() << mu::rotateY(mu::Degree(-90.f));
}

void MU_CALLCONV Player::addCamera( mu::Vec3 offset, float timeLag,
	const gfx::d3d12::Camera::Config& config, gfx::d3d12engine::CoordRoot& coordRoot
) {
	createComponent<gfx::d3d12engine::Camera>(config);
	auto& camera = as<gfx::d3d12engine::Camera>();
	camera.get().coordMovement() << mu::translate(offset);
	camera.get().coordRotation().setLocalXform(
		mu::transpose(mu::lookAt(mu::Vec3(), -offset, mu::Vec3(0.f, 1.f, 0.f)))
	);
	camera.get().coordMovement().setParent(&coordRoot.get());
	camera.setOffset(offset);
	camera.setTimeLag(timeLag);
	camera.attach( as<gfx::d3d12engine::Model>() );
}

void Player::update(float deltaTime) {
	as<gfx::d3d12engine::Coord>().get()
		<< mu::translate( as<RigidBody>().deltaPosition() );
}

void Player::postUpdate(float deltaTime) {
	as<gfx::d3d12engine::Camera>().update(deltaTime);
	as<gfx::d3d12engine::Camera>().get().updateView();
}