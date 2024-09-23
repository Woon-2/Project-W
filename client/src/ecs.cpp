#include "ecs.hpp"

#include <ranges>
#include <algorithm>

namespace ecs {
    void init(const InitDesc& desc) {
        Entity::init(desc.threadCnt, desc.entityPoolSize);
        Component::init(desc.threadCnt * desc.entityPoolSize);
    }

    void Entity::init(std::size_t threadCnt, std::size_t entityPoolSize) {
        sEntityPools.clear();

        std::ranges::generate_n( std::back_inserter(sEntityPools), threadCnt, [entityPoolSize]() {
            auto pool = std::deque<ID>();
            std::ranges::generate_n( std::back_inserter(pool), entityPoolSize, [n = 0]() mutable {
                return n++;
            } );
            return pool;
        } );
    }

    std::size_t Entity::poolIdx(std::thread::id threadId) {
        if (sThreadMap.contains(threadId)) {
            return sThreadMap[threadId];
        }
        else {
            while (sLock.test_and_set(std::memory_order_acquire)) {
                while (sLock.test(std::memory_order_relaxed)) {
                    std::this_thread::yield();
                }
            }

            if (auto it = sThreadMap.find(threadId); it != sThreadMap.end()) {
                sLock.clear(std::memory_order_release);
                return it->second;
            }

            auto& ret = sThreadMap[threadId] = sThreadMap.size();
            sLock.clear(std::memory_order_release);
            return ret;
        }
    }

    Entity::ID Entity::fetch(std::thread::id threadId) {
        auto idx = poolIdx(threadId);
        ID id = sEntityPools[idx].front();
        sEntityPools[idx].pop_front();
        return id;
    }

    std::vector<Entity::ID> Entity::fetch(std::thread::id threadId, std::size_t cnt) {
        auto idx = poolIdx(threadId);

        std::vector<ID> ids;
        ids.reserve(cnt);

        for (auto id : sEntityPools[idx] | std::views::take(cnt)) {
            ids.push_back(id);
        }

        sEntityPools[idx].erase(sEntityPools[idx].begin(), sEntityPools[idx].begin() + cnt);

        return ids;
    }

    std::vector<std::deque<Entity::ID>> Entity::sEntityPools;
    std::map<std::thread::id, std::size_t> Entity::sThreadMap;
    std::atomic_flag Entity::sLock;

    std::vector< std::vector<std::shared_ptr<Component> > > Component::sComponents;
}