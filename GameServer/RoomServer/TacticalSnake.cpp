#include "rspch.hpp"
#include "TacticalSnake.hpp"

TacticalNpcConfig TacticalSnake::trooperConfig() {
	return TacticalNpcConfig{
		.maxHp             = 45.f,
		.moveSpeed         = 8.f,
		.attackRange       = 2.6f,
		.attackDamage      = 12.f,
		.attackWindupTime  = 0.35s,
		.attackRecoverTime = 0.8s,
		.separationRadius  = 3.f,
		.separationWeight  = 0.9f
	};
}
