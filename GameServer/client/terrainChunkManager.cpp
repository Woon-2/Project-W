#include "pch.hpp"
#include "terrainChunkManager.hpp"
#include "gfx.hpp"
#include "physicsWorld.hpp"
#include "threadPool.hpp"

#include <cmath>
#include <chrono>

void TerrainChunkManager::init(GFX& gfx, PhysicsWorld& physicsWorld, ThreadPool* threadPool,
                               const std::filesystem::path& terrainDir) {
    gfx_          = &gfx;
    physicsWorld_ = &physicsWorld;
    threadPool_   = threadPool;
    terrainDir_   = terrainDir;

    index_ = parseChunkIndex(terrainDir_);
    if (index_.chunks.empty()) {
        gSharedLog << "[ChunkManager] No chunks found in index.\n";
        return;
    }

    chunkSizeX_ = index_.chunks.front().sizeX;
    chunkSizeZ_ = index_.chunks.front().sizeZ;

    indexByCoord_.clear();
    for (const auto& e : index_.chunks)
        indexByCoord_[packCoord(e.col, e.row)] = &e;

    // Load the shared layer palette once on the main thread (touches DescriptorPool +
    // texHashMap). Chunks themselves stream in on demand via update().
    gfx_->recordTerrainResourceLoad(
        [&](ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
            DescriptorPool& texPool, Fence& fence) {
            palette_ = loadLayerPalette(index_, terrainDir_, device, cmdList,
                                        texHashMap_, texPool, fence);
        },
        /*wait=*/true);

    gSharedLog << "[ChunkManager] Initialized: " << index_.chunks.size()
               << " chunks indexed, palette loaded. Streaming on demand.\n";
}

void TerrainChunkManager::update(mu::Vec3 playerWorldPos, Milliseconds dt) {
    if (index_.chunks.empty()) return;

    const float dtSec = std::chrono::duration<float>(dt).count();

    // Resolve the player's current chunk; keep the last known one if the player is
    // momentarily over a hole / outside the populated region.
    auto cur = chunkCoordAtWorld(playerWorldPos.x(), playerWorldPos.z());
    if (cur) lastCur_ = cur;

    std::unordered_set<int64_t> desired;
    int64_t curKey = static_cast<int64_t>(-1);
    if (lastCur_) {
        desired = computeDesired(lastCur_->first, lastCur_->second);
        curKey  = packCoord(lastCur_->first, lastCur_->second);
    }

    int loading = 0;
    for (auto& [k, s] : chunks_)
        if (s.state == State::Loading) ++loading;

    // Start loads for desired chunks not yet present; cancel pending expiry for ones
    // that re-entered the desired set.
    for (int64_t key : desired) {
        auto it = chunks_.find(key);
        if (it == chunks_.end()) {
            if (loading < maxConcurrentLoads_) {
                startLoad(*indexByCoord_.at(key));
                ++loading;
            }
        } else if (it->second.state == State::Expiring) {
            it->second.state = State::Ready;
            it->second.expireSeconds = 0.f;
        }
    }

    // Expire / unload chunks outside the desired set (never the chunk underfoot).
    std::vector<int64_t> toUnload;
    for (auto& [key, slot] : chunks_) {
        if (key == curKey) {
            if (slot.state == State::Expiring) { slot.state = State::Ready; slot.expireSeconds = 0.f; }
            continue;
        }
        if (desired.count(key) > 0) continue;

        if (slot.state == State::Ready) {
            slot.state = State::Expiring;
            slot.expireSeconds = 0.f;
        }
        if (slot.state == State::Expiring) {
            slot.expireSeconds += dtSec;
            if (slot.expireSeconds >= graceSeconds_) toUnload.push_back(key);
        }
        // Loading chunks outside desired are left to finish; they expire next pass.
    }
    for (int64_t key : toUnload) {
        auto it = chunks_.find(key);
        if (it == chunks_.end()) continue;
        unloadChunk(it->second);
        chunks_.erase(it);
    }

    drainCompletedBuilds();
    tickGraveyard();
}

std::unordered_set<int64_t>
TerrainChunkManager::computeDesired(int curCol, int curRow) const {
    std::unordered_set<int64_t> visited;
    const int64_t startKey = packCoord(curCol, curRow);
    if (indexByCoord_.find(startKey) == indexByCoord_.end()) return visited;

    std::vector<std::pair<int64_t, int>> queue;   // (key, hop depth)
    queue.push_back({ startKey, 0 });
    visited.insert(startKey);

    for (std::size_t qi = 0; qi < queue.size(); ++qi) {
        const auto [key, depth] = queue[qi];
        if (depth >= maxHop_) continue;
        auto it = indexByCoord_.find(key);
        if (it == indexByCoord_.end()) continue;
        for (const auto& [nc, nr] : it->second->neighbors) {
            const int64_t nkey = packCoord(nc, nr);
            if (indexByCoord_.find(nkey) == indexByCoord_.end()) continue;
            if (visited.insert(nkey).second)
                queue.push_back({ nkey, depth + 1 });
        }
    }
    return visited;
}

void TerrainChunkManager::startLoad(const ChunkIndexEntry& entry) {
    const int64_t key = packCoord(entry.col, entry.row);
    auto& slot = chunks_[key];
    slot.entry = entry;
    slot.state = State::Loading;
    slot.expireSeconds = 0.f;
    slot.cpu = std::make_unique<ChunkCpuBuild>();

    ChunkCpuBuild*         cpuPtr = slot.cpu.get();
    const ChunkIndexEntry* ePtr   = indexByCoord_.at(key);

    // The CPU build (height parse + vertex/normal/tangent/index/heightField generation)
    // touches no shared mutable state, so it is safe on a worker thread.
    if (threadPool_) {
        threadPool_->addJob([cpuPtr, ePtr, dir = terrainDir_]() {
            buildChunkCpu(*ePtr, dir, *cpuPtr);
        });
    } else {
        buildChunkCpu(*ePtr, terrainDir_, *cpuPtr);
    }
}

void TerrainChunkManager::drainCompletedBuilds() {
    int budget = maxGpuFinalizePerFrame_;
    std::vector<int64_t> toErase;

    for (auto& [key, slot] : chunks_) {
        if (budget <= 0) break;
        if (slot.state != State::Loading || !slot.cpu) continue;
        if (!slot.cpu->done.load(std::memory_order_acquire)) continue;

        if (slot.cpu->failed) {
            toErase.push_back(key);
            continue;
        }

        // GPU finalize on the main thread (brief wait reclaims the upload buffers).
        ChunkCpuBuild* cpuPtr = slot.cpu.get();
        gfx_->recordTerrainResourceLoad(
            [&](ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
                DescriptorPool& texPool, Fence& fence) {
                slot.data = finalizeChunkGpu(*cpuPtr, palette_, terrainDir_, device, cmdList,
                                             texHashMap_, texPool, fence);
            },
            /*wait=*/true);

        slot.cpu.reset();
        activateChunkRenderAndPhysics(slot);   // sets State::Ready
        --budget;
    }

    for (int64_t key : toErase) chunks_.erase(key);
}

void TerrainChunkManager::activateChunkRenderAndPhysics(LoadedChunk& slot) {
    const mu::Vec3 offset = worldOffset(slot.entry.col, slot.entry.row);

    auto obj = std::make_shared<TerrainObject>();
    obj->body().setMotionType(MotionType::Static);
    obj->setPos(offset);
    obj->setTerrainData(&slot.data);
    obj->activateOcclusion(true);
    obj->update(Milliseconds(0), 1.f);   // populate renderState_.world (chunk is static)

    slot.object = std::move(obj);

    if (physicsWorld_ && !slot.data.heightField.empty()) {
        slot.physHandle = physicsWorld_->registerTerrain(
            &slot.object->body(), &slot.data.heightField);
    }
    slot.state = State::Ready;
}

void TerrainChunkManager::unloadChunk(LoadedChunk& slot) {
    if (physicsWorld_ && slot.physHandle != static_cast<std::size_t>(-1)) {
        physicsWorld_->unregisterTerrain(slot.physHandle);
        slot.physHandle = static_cast<std::size_t>(-1);
    }

    PendingUnload pu;
    pu.framesLeft  = graveyardFrameDelay_;
    pu.splatSrvIdx = (slot.data.splatMap.idxSrv.idxResource >= 0)
                   ? slot.data.splatMap.idxSrv.idxResource : -1;

    // Move the splat GPU resource owner (the texHashMap_ entry) into the graveyard so
    // it is released only after the GPU is no longer referencing it. Palette textures
    // are shared and are NOT freed here.
    const std::string splatKey = "Terrain_Splat_" + std::to_string(slot.entry.col)
                               + "_" + std::to_string(slot.entry.row);
    auto tit = texHashMap_.find(splatKey);
    if (tit != texHashMap_.end()) {
        pu.retainedTex = std::move(tit->second);
        texHashMap_.erase(tit);
    }

    pu.data   = std::move(slot.data);
    pu.object = std::move(slot.object);
    graveyard_.push_back(std::move(pu));
}

void TerrainChunkManager::tickGraveyard() {
    for (auto& pu : graveyard_) --pu.framesLeft;

    for (std::size_t i = 0; i < graveyard_.size();) {
        if (graveyard_[i].framesLeft <= 0) {
            if (graveyard_[i].splatSrvIdx >= 0 && gfx_)
                gfx_->bindlessTexPool().free(graveyard_[i].splatSrvIdx);
            graveyard_[i] = std::move(graveyard_.back());
            graveyard_.pop_back();
        } else {
            ++i;
        }
    }
}

void TerrainChunkManager::submitDrawEvents(GFX& gfx) {
    for (auto& [key, slot] : chunks_) {
        if ((slot.state == State::Ready || slot.state == State::Expiring) && slot.object)
            slot.object->render(gfx);
    }
}

const TerrainChunkManager::LoadedChunk*
TerrainChunkManager::findReady(int col, int row) const {
    auto it = chunks_.find(packCoord(col, row));
    if (it == chunks_.end()) return nullptr;
    if (it->second.state != State::Ready && it->second.state != State::Expiring) return nullptr;
    return &it->second;
}

std::optional<std::pair<int,int>>
TerrainChunkManager::chunkCoordAtWorld(float x, float z) const {
    if (chunkSizeX_ <= 0.f || chunkSizeZ_ <= 0.f) return std::nullopt;
    const int col = static_cast<int>(std::floor(x / chunkSizeX_));
    const int row = static_cast<int>(std::floor(z / chunkSizeZ_));
    if (indexByCoord_.find(packCoord(col, row)) == indexByCoord_.end())
        return std::nullopt;
    return std::make_pair(col, row);
}

float MU_CALLCONV TerrainChunkManager::heightAtWorld(float x, float z) const {
    auto coord = chunkCoordAtWorld(x, z);
    if (!coord) return 0.f;
    const LoadedChunk* c = findReady(coord->first, coord->second);
    if (!c) return 0.f;
    const mu::Vec3 off = worldOffset(coord->first, coord->second);
    return off.y() + c->data.heightField.getHeightAt(x - off.x(), z - off.z());
}

mu::Vec3 MU_CALLCONV TerrainChunkManager::normalAtWorld(float x, float z) const {
    auto coord = chunkCoordAtWorld(x, z);
    if (!coord) return mu::Vec3(0.f, 1.f, 0.f);
    const LoadedChunk* c = findReady(coord->first, coord->second);
    if (!c) return mu::Vec3(0.f, 1.f, 0.f);
    const mu::Vec3 off = worldOffset(coord->first, coord->second);
    return c->data.heightField.getNormalAt(x - off.x(), z - off.z());
}
