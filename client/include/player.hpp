#ifndef __player_HPP
#define __player_HPP

#include "ecs.hpp"
#include "d3d12engine/d3d12Engine.hpp"

class Player : public ecs::Entity {
public:
	void init(gfx::d3d12engine::Core& core);
	void update();
};

#endif	// __player_HPP

