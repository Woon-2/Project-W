#include "rspch.hpp"
#include "stronghold.hpp"
#include "goblin.hpp"
#include "snake.hpp"
#include "mushroom.hpp"
#include "Room.hpp"
#include <algorithm>
#include <cmath>
#include <random>

static thread_local std::mt19937 s_strongholdRng{ std::random_device{}() };

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

mu::Vec3 Stronghold::randomSpawnPos(Room& room) const {
    std::uniform_real_distribution<float> distR(0.f, 1.f);
    std::uniform_real_distribution<float> distAngle(0.f, 2.f * DirectX::XM_PI);

    const float r     = def_.spawnRadius * std::sqrt(distR(s_strongholdRng));
    const float theta = distAngle(s_strongholdRng);
    const float x = def_.center.x() + r * std::cos(theta);
    const float z = def_.center.z() + r * std::sin(theta);
    const float y = room.groundHeightAtWorld(x, z);
    return mu::Vec3(x, y, z);
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
            m.reviveAt(randomSpawnPos(room));
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
