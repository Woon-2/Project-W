#include "rspch.hpp"
#include "TacticalGoblin.hpp"

TacticalNpcConfig TacticalGoblin::trooperConfig() {
	auto cfg = TacticalNpcConfig{
		.maxHp              = 60.f,
		.moveSpeed          = 4.f,
		.animRefSpeed       = 3.0f,   // Goblin_Walk authored speed (not measured -- default; same clip set as the field Goblin)
		.attackRange        = 2.8f,
		.attackDamage       = 12.f,
		.attackWindupTime   = 0.4s,
		.attackRecoverTime  = 0.8s,
		.separationRadius   = 3.f,
		.separationWeight   = 0.5f
    };
    return cfg;
}

TacticalNpcConfig TacticalGoblin::bossConfig() {
    auto cfg = TacticalNpcConfig{
		.maxHp				= 2000.f,
		.moveSpeed			= 5.5f,
		.animRefSpeed		= 3.0f,   // Hobgoblin reuses the Goblin clip set (Room.cpp objType switch)
		.attackRange		= 3.5f,
		.attackDamage		= 40.f,
		.attackWindupTime	= 0.35s,
		.attackRecoverTime	= 0.6s,
		.separationRadius	= 4.f,
		.separationWeight	= 0.3f,
		.attackDamageScale	= 3.0f   // boss uses the baseline Goblin skill but hits ~3x harder
    };
    return cfg;
}
