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
	as<Model>().init(
		assetSystem.model("DragonModel"),
		assetSystem.materialTree("DragonMaterialTree")
	);
}