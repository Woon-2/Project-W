#include "rspch.hpp"
#include "TacticalBomber.hpp"

TacticalNpcConfig TacticalBomber::trooperConfig() {
	return TacticalNpcConfig{
		.maxHp             = 45.f,
		.moveSpeed         = 5.f,
		.attackRange       = 2.6f,
		.attackDamage      = 8.f,
		.attackWindupTime  = 0.35s,
		.attackRecoverTime = 0.8s,
		.separationRadius  = 3.f,
		.separationWeight  = 0.9f
	};
}
