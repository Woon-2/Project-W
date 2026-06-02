#include "rspch.hpp"
#include "stronghold.hpp"
#include "goblin.hpp"
#include "Room.hpp"
#include <algorithm>
#include <cmath>
#include <random>

static thread_local std::mt19937 s_strongholdRng{ std::random_device{}() };

void Stronghold::configure(const StrongholdDef& def, int groupId, int poolStart, int poolCount) {
    def_             = def;
    groupId_         = groupId;
    poolStart_       = poolStart;
    poolCount_       = poolCount;
    strongholdMaxHp_ = def.maxHp;

    // Resolve the Goblin population entry (only Goblin is implemented for now).
    for (const auto& pop : def.populations) {
        if (pop.type == ObjectType::Goblin) {
            goblinMaxPerWave_      = std::max(1, pop.maxPerWave);
            goblinRespawnInterval_ = pop.respawnInterval;
            break;
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

void Stronghold::updatePopulation(Seconds dt, std::vector<Goblin>& pool, Room& room,
                                  std::vector<uint32>& outRevivedIds) {
    if (destroyed_) return;

    goblinRespawnTimer_ += dt;
    if (goblinRespawnTimer_ < goblinRespawnInterval_) return;
    goblinRespawnTimer_ = Seconds{ 0.f };

    int revived = 0;
    const int end = poolStart_ + poolCount_;
    for (int i = poolStart_; i < end && revived < goblinMaxPerWave_; ++i) {
        Goblin& g = pool[static_cast<size_t>(i)];
        if (g.hp() > 0) continue;   // alive, keep
        g.reviveAt(randomSpawnPos(room));
        outRevivedIds.push_back(g.getId());
        ++revived;
    }
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
