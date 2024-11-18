#ifndef __player_HPP
#define __player_HPP

#include "ecs.hpp"

class Player : public ecs::Entity {
public:
	Player();

	void update();
};

#endif	// __player_HPP

