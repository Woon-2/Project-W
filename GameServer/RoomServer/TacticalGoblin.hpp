#pragma once
#include "TacticalNpc.hpp"
#include "mathUtil.hpp"

class Room;

class TacticalGoblin : public TacticalNpc {
public:
    TacticalGoblin() = default;
    explicit TacticalGoblin(Object&& base) : TacticalNpc(std::move(base), trooperConfig()) {}

    static TacticalNpcConfig trooperConfig();
    static TacticalNpcConfig bossConfig();

    // Spawns a full goblin boss encounter into room: one PlatoonLeader boss at
    // bossPos plus numSquads squads of troopersPerSquad surrounding spawnCenter.
    static void MU_CALLCONV spawnEncounter(Room& room, mu::Vec3 spawnCenter,
                                           mu::Vec3 bossPos,
                                           int numSquads     = 3,
                                           int troopersPerSquad = 20);
};
