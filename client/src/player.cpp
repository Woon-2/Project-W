#include "player.hpp"

#include "inputSystem.hpp"
#include "physicsSystem.hpp"

#include "d3d12engine/d3d12Engine.hpp"

Player::Player() {
	createComponent<RigidBody>();
	createComponent<PlayerController>();
	// createComponent<gfx::d3d12engine::Model>();
	createComponent<gfx::d3d12engine::Coord>();
}

void Player::update() {
	as<gfx::d3d12engine::Coord>().get()
		<< mu::translate( as<RigidBody>().deltaPosition() );
}