#include "rspch.hpp"
#include "TacticalBirdy.hpp"

TacticalNpcConfig TacticalBirdy::trooperConfig() {
	return TacticalNpcConfig{
		.maxHp             = 80.f,
		.moveSpeed         = 4.f,
		.attackRange       = 2.6f,
		.attackDamage      = 10.f,
		.attackWindupTime  = 0.35s,
		.attackRecoverTime = 0.8s,
		.separationRadius  = 3.f,
		.separationWeight  = 1.0f
	};
}
