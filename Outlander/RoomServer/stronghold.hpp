#ifndef room_server_stronghold_hpp
#define room_server_stronghold_hpp

#include "object.hpp"
#include "strongholdDef.hpp"
#include <vector>

class Room;
class Goblin;
class Snake;
class Mushroom;
class Bomber;
class Birdy;
class Slime;
class Treant;

// A stronghold: a damageable static structure that also maintains fixed monster
// pools around it at the configured population (the "fixed pool revive" model).
// The structure participates in combat as a normal damageable Object
// (Faction::Monsters, canReceiveDamage, Static collider) so player skills can
// destroy it; while destroyed it stops reviving its garrison, and rebuilds after
// a delay.
class Stronghold : public Object {
public:
    Stronghold() = default;

    // Per-type pool tracking inside the owning Room's monster vectors.
    struct MonsterPool {
        int     start           = 0;
        int     count           = 0;
        int     maxPerWave      = 1;
        Seconds respawnInterval { 5.f };
        Seconds respawnTimer    { 0.f };
    };

    // Bind this stronghold to its config, NpcGroup id, and per-type pool ranges.
    void configure(const StrongholdDef& def, int groupId,
                   int goblinStart,   int goblinCount,
                   int snakeStart,    int snakeCount,
                   int mushroomStart, int mushroomCount,
                   int bomberStart,   int bomberCount,
                   int birdyStart,    int birdyCount,
                   int slimeStart,    int slimeCount,
                   int treantStart,   int treantCount);

    // Revive dead monsters in all pool ranges, up to maxPerWave per type per
    // respawn interval (no revival while destroyed).
    void updatePopulation(Seconds dt,
                          std::vector<Goblin>&   goblins,
                          std::vector<Snake>&    snakes,
                          std::vector<Mushroom>& mushrooms,
                          std::vector<Bomber>&   bombers,
                          std::vector<Birdy>&    birdys,
                          std::vector<Slime>&    slimes,
                          std::vector<Treant>&   treants,
                          Room& room,
                          std::vector<uint32>& outRevivedIds);

    // Drive the structure destruction/rebuild timeline. Returns true when the
    // alive<->destroyed state changed this tick (room should broadcast state).
    bool updateStructure(Seconds dt);

    bool                 isDestroyed()      const { return destroyed_; }
    int                  groupId()          const { return groupId_; }
    int                  strongholdMaxHp()  const { return strongholdMaxHp_; }
    const StrongholdDef& def()              const { return def_; }

private:
    StrongholdDef def_{};
    int groupId_ = -1;

    MonsterPool goblinPool_;
    MonsterPool snakePool_;
    MonsterPool mushroomPool_;
    MonsterPool bomberPool_;
    MonsterPool birdyPool_;
    MonsterPool slimePool_;
    MonsterPool treantPool_;

    int     strongholdMaxHp_ = 1000;
    bool    destroyed_       = false;
    Seconds rebuildTimer_    { 0.f };
};

#endif // room_server_stronghold_hpp
