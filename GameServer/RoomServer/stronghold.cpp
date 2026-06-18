#include "rspch.hpp"
#include "stronghold.hpp"
#include "goblin.hpp"
#include "snake.hpp"
#include "mushroom.hpp"
#include "Room.hpp"
#include <algorithm>

void Stronghold::configure(const StrongholdDef& def, int groupId,
                           int goblinStart, int goblinCount,
                           int snakeStart,  int snakeCount,
                           int mushroomStart, int mushroomCount)
{
    def_             = def;
    groupId_         = groupId;
    strongholdMaxHp_ = def.maxHp;

    goblinPool_.start   = goblinStart;
    goblinPool_.count   = goblinCount;
    snakePool_.start    = snakeStart;
    snakePool_.count    = snakeCount;
    mushroomPool_.start = mushroomStart;
    mushroomPool_.count = mushroomCount;

    for (const auto& pop : def.populations) {
        MonsterPool* pool = nullptr;
        switch (pop.type) {
        case ObjectType::Goblin:   pool = &goblinPool_;   break;
        case ObjectType::Snake:    pool = &snakePool_;    break;
        case ObjectType::Mushroom: pool = &mushroomPool_; break;
        default: break;
        }
        if (pool) {
            pool->maxPerWave      = std::max(1, pop.maxPerWave);
            pool->respawnInterval = pop.respawnInterval;
        }
    }
}

void Stronghold::updatePopulation(Seconds dt,
                                  std::vector<Goblin>&   goblins,
                                  std::vector<Snake>&    snakes,
                                  std::vector<Mushroom>& mushrooms,
                                  Room& room,
                                  std::vector<uint32>&   outRevivedIds)
{
    if (destroyed_) return;

    auto tryRevive = [&](MonsterPool& pool, auto& vec) {
        if (pool.count == 0) return;
        pool.respawnTimer += dt;
        if (pool.respawnTimer < pool.respawnInterval) return;
        pool.respawnTimer = Seconds{ 0.f };
        int revived = 0;
        const int end = pool.start + pool.count;
        for (int i = pool.start; i < end && revived < pool.maxPerWave; ++i) {
            auto& m = vec[static_cast<size_t>(i)];
            if (m.hp() > 0) continue;
            m.reviveAt(room.randomSpawnInDiscAvoidingProps(def_.center, def_.spawnRadius, m));
            outRevivedIds.push_back(m.getId());
            ++revived;
        }
    };

    tryRevive(goblinPool_,   goblins);
    tryRevive(snakePool_,    snakes);
    tryRevive(mushroomPool_, mushrooms);
}

bool Stronghold::updateStructure(Seconds dt) {
    if (!destroyed_) {
        if (hp() <= 0) {
            destroyed_    = true;
            rebuildTimer_ = Seconds{ 0.f };
            return true;            // alive -> destroyed
        }
        return false;
    }

    rebuildTimer_ += dt;
    if (rebuildTimer_ >= def_.respawnDelay) {
        destroyed_ = false;
        setHp(strongholdMaxHp_);
        return true;                // destroyed -> alive (rebuilt)
    }
    return false;
}
