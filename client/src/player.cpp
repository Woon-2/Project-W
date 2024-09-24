#include "player.hpp"

#include "inputSystem.hpp"
#include "physicsSystem.hpp"
#include "assetSystem.hpp"
#include "ccoord.hpp"
#include "cmodel.hpp"

#include "assetMap.hpp"

Player::Player() {
	createComponent<RigidBody>();
	createComponent<AssetLinker>();
	createComponent<PlayerController>();
	createComponent<Model>();
	createComponent<Coord>();
	as<AssetLinker>().configAsset(assetIDs::dragon);
}

void Player::linkAssets(const AssetSystem& assetSystem) {
	auto& model = as<Model>();
	model.init(
		assetSystem.model("DragonModel"),
		assetSystem.materialTree("DragonMaterialTree")
	);
	model.root().coord().setParent(&as<Coord>().get());
}

void Player::update() {
	as<Coord>().get() << mu::translate( as<RigidBody>().deltaPosition() );
}