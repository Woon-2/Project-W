#include "rspch.hpp"
#include "TacticalTreant.hpp"

TacticalNpcConfig TacticalTreant::trooperConfig() {
	return TacticalNpcConfig{
		.maxHp             = 150.f,
		.moveSpeed         = 3.f,
		.attackRange       = 3.0f,
		.attackDamage      = 20.f,
		.attackWindupTime  = 0.5s,
		.attackRecoverTime = 1.0s,
		.separationRadius  = 3.5f,
		.separationWeight  = 0.6f
	};
}
