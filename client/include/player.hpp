#ifndef __PLAYERSYSTEM_HPP
#define __PLAYERSYSTEM_HPP

#include "ecs.hpp"

#include "physicsSystem.hpp"

class Player : public ecs::Entity {
public:
	Player();

	void linkAssets();
};

#endif // !__PLAYERSYSTEM_HPP

