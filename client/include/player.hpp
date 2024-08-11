#ifndef __PLAYERSYSTEM_HPP
#define __PLAYERSYSTEM_HPP

#include "ecs.hpp"

struct Position
{
	double x;
	double y;
	double z;
};

class Player
{
public:
	Player() : entityNumber_(0) {}
	void Init();

	void setPosition(double x, double y, double z) {
		auto& position = ecs::GetComponent<Position>(entityNumber_);
		position.x += x;	
		position.y = y;	
		position.z = z;
	}
	void printPos() {
		std::cout << "x : " << ecs::GetComponent<Position>(entityNumber_).x << std::endl;
	}

	ecs::Entity entityNumber_;
};

#endif // !__PLAYERSYSTEM_HPP

