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

	void printPos() 
	{
		std::cout << "\033[2J\033[1;1H";

		auto curPos = ecs::GetComponent<Position>(entityNumber_);
		std::cout << "x : " << curPos.x << " y :" << curPos.y << std::endl;
	}

	ecs::Entity entityNumber_;
};

#endif // !__PLAYERSYSTEM_HPP

