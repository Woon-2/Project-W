#include "rspch.hpp"
#include "TacticalSlime.hpp"

TacticalNpcConfig TacticalSlime::trooperConfig() {
	return TacticalNpcConfig{
		.maxHp             = 60.f,
		.moveSpeed         = 2.5f,
		.attackRange       = 2.6f,
		.attackDamage      = 8.f,
		.attackWindupTime  = 0.35s,
		.attackRecoverTime = 0.8s,
		.separationRadius  = 3.f,
		.separationWeight  = 1.0f
	};
}
