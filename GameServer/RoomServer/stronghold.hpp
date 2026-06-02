#ifndef room_server_stronghold_hpp
#define room_server_stronghold_hpp

#include "object.hpp"
#include "strongholdDef.hpp"
#include <vector>

class Room;
class Goblin;

// A stronghold: a damageable static structure that also maintains a fixed pool
// of monsters around it at the configured population (the "fixed pool revive"
// model). The structure participates in combat as a normal damageable Object
// (Faction::Monsters, canReceiveDamage, Static collider) so player skills can
// destroy it; while destroyed it stops reviving its garrison, and rebuilds after
// a delay.
class Stronghold : public Object {
public:
    Stronghold() = default;

    // Bind this stronghold to its config, shared NpcGroup id, and its contiguous
    // range [poolStart, poolStart+poolCount) inside the room's goblin pool.
    void configure(const StrongholdDef& def, int groupId, int poolStart, int poolCount);

    // Revive dead monsters in the pool range, up to maxPerWave per respawn
    // interval (no revival while destroyed). Appends revived monster ids so the
    // room can broadcast S_NpcRespawn for them.
    void updatePopulation(Seconds dt, std::vector<Goblin>& pool, Room& room,
                          std::vector<uint32>& outRevivedIds);

    // Drive the structure destruction/rebuild timeline. Returns true when the
    // alive<->destroyed state changed this tick (room should broadcast state).
    bool updateStructure(Seconds dt);

    bool                 isDestroyed()      const { return destroyed_; }
    int                  groupId()          const { return groupId_; }
    int                  strongholdMaxHp()  const { return strongholdMaxHp_; }
    const StrongholdDef& def()              const { return def_; }

private:
    mu::Vec3 randomSpawnPos(Room& room) const;

    StrongholdDef def_{};
    int groupId_   = -1;
    int poolStart_ = 0;
    int poolCount_ = 0;

    // Resolved Goblin population parameters (only Goblin is implemented for now).
    int     goblinMaxPerWave_      = 1;
    Seconds goblinRespawnInterval_{ 5.f };
    Seconds goblinRespawnTimer_   { 0.f };

    int     strongholdMaxHp_ = 1000;
    bool    destroyed_       = false;
    Seconds rebuildTimer_    { 0.f };
};

#endif // room_server_stronghold_hpp
