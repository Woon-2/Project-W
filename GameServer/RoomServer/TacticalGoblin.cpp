#include "rspch.hpp"
#include "TacticalGoblin.hpp"
#include "Room.hpp"

TacticalNpcConfig TacticalGoblin::trooperConfig() {
    TacticalNpcConfig cfg;
    cfg.maxHp             = 60.f;
    cfg.moveSpeed         = 4.f;
    cfg.attackRange       = 2.8f;
    cfg.attackDamage      = 12.f;
    cfg.attackWindupTime  = 0.4f;
    cfg.attackRecoverTime = 0.8f;
    cfg.separationRadius  = 3.f;
    cfg.separationWeight  = 0.5f;
    return cfg;
}

TacticalNpcConfig TacticalGoblin::bossConfig() {
    TacticalNpcConfig cfg;
    cfg.maxHp             = 2000.f;
    cfg.moveSpeed         = 5.5f;
    cfg.attackRange       = 3.5f;
    cfg.attackDamage      = 40.f;
    cfg.attackWindupTime  = 0.35f;
    cfg.attackRecoverTime = 0.6f;
    cfg.separationRadius  = 4.f;
    cfg.separationWeight  = 0.3f;
    return cfg;
}

void TacticalGoblin::spawnEncounter(Room& room, mu::Vec3 spawnCenter,
                                    mu::Vec3 bossPos,
                                    int numSquads, int troopersPerSquad) {
    room.spawnTacticalGoblinEncounter(spawnCenter, bossPos, numSquads, troopersPerSquad);
}
