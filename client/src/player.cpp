#include "player.hpp"

#include "inputSystem.hpp"
#include "physicsSystem.hpp"
#include "assetSystem.hpp"
#include "ccoord.hpp"
#include "cmodel.hpp"

Player::Player() {
	createComponent<RigidBody>();
	createComponent<AssetLinker>();
	createComponent<PlayerController>();
	createComponent<Model>();
	createComponent<Coord>();
}

void Player::linkAssets() {
	
}