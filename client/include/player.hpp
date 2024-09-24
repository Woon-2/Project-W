#ifndef __PLAYERSYSTEM_HPP
#define __PLAYERSYSTEM_HPP

#include "ecs.hpp"

#include "assetSystem.hpp"

class Player : public ecs::Entity {
public:
	Player();

	void linkAssets(const AssetSystem& assetSystem);
};

#endif // !__PLAYERSYSTEM_HPP

