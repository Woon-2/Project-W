#ifndef __terrainChunkManager_HPP
#define __terrainChunkManager_HPP

#include "terrain.hpp"
#include "object.hpp"      // TerrainObject

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <optional>
#include <memory>
#include <cstdint>
#include <filesystem>

class GFX;
class PhysicsWorld;
class ThreadPool;

// Owns the unified terrain layer palette, the global chunk index, and the set of
// currently loaded chunks. Replaces the former single TerrainData/TerrainObject.
//
// Coordinate model (sparse + arbitrary origin):
//   - col,row are absolute signed grid indices; the chunk set may be sparse and
//     need not include (0,0). Cell size is uniform.
//   - worldOffset(col,row) = (col * chunkSizeX, 0, row * chunkSizeZ).
//   - chunkCoordAtWorld(x,z) = (floor(x/chunkSizeX), floor(z/chunkSizeZ)).
//
// Phase 3 baseline: init() loads the palette + every chunk synchronously.
// Async streaming (worker-thread CPU build, fence-gated activation) and the
// hop-based load/unload lifecycle are layered on in later phases via update().
class TerrainChunkManager {
public:
    void init(GFX& gfx, PhysicsWorld& physicsWorld, ThreadPool* threadPool,
              const std::filesystem::path& terrainDir);

    // Per-frame streaming tick (Phase 6 fills this in; no-op for the baseline).
    void update(mu::Vec3 playerWorldPos, Milliseconds dt);

    // Submits a render draw event (+ Hi-Z occluder) for every ready chunk.
    void submitDrawEvents(GFX& gfx);

    // World-space height/normal routed to the chunk containing (x,z).
    // Returns 0 / +Y when no loaded chunk covers the point.
    float    MU_CALLCONV heightAtWorld(float x, float z) const;
    mu::Vec3 MU_CALLCONV normalAtWorld(float x, float z) const;

    // Grid coord of the chunk containing (x,z), if such a chunk exists in the index.
    std::optional<std::pair<int,int>> chunkCoordAtWorld(float x, float z) const;

    // True when no terrain exists at all (index had no chunks). Stays valid during
    // streaming latency so callers submit terrain frame data / height queries consistently.
    bool empty() const { return index_.chunks.empty(); }

    // Zone (trigger volume) definitions parsed from chunks_index.bin.
    const std::vector<ZoneDef>& zones() const { return index_.zones; }

    // Generic placement markers parsed from chunks_index.bin.
    const std::vector<MarkerDef>& markers() const { return index_.markers; }

    // Returns how much of the streamed chunk set around worldPos is fully activated.
    // Used by scene loading gates so the first visible frame is not missing terrain.
    float readyFractionAround(mu::Vec3 worldPos) const;
    bool readyAround(mu::Vec3 worldPos) const { return readyFractionAround(worldPos) >= 1.f; }

    // 인덱스의 모든 청크 중심을 평균한 월드 좌표(지형 위 한 점).
    // 로비 대기실의 정적 카메라 포커스로 사용한다(sparse/임의 원점 대응).
    mu::Vec3 MU_CALLCONV worldCenter() const {
        if (index_.chunks.empty()) return mu::Vec3(0.f, 0.f, 0.f);
        double sx = 0.0, sz = 0.0;
        for (const auto& c : index_.chunks) {
            sx += static_cast<double>(c.col) * chunkSizeX_ + chunkSizeX_ * 0.5;
            sz += static_cast<double>(c.row) * chunkSizeZ_ + chunkSizeZ_ * 0.5;
        }
        const float cx = static_cast<float>(sx / index_.chunks.size());
        const float cz = static_cast<float>(sz / index_.chunks.size());
        return mu::Vec3(cx, heightAtWorld(cx, cz), cz);
    }

private:
    // Loading(CPU): worker-thread build in flight. Ready: render object + physics active.
    // Expiring: Ready but outside the desired set, counting down its grace timer.
    enum class State { Unloaded, Loading, Ready, Expiring };

    struct LoadedChunk {
        ChunkIndexEntry entry;
        State           state = State::Unloaded;
        std::unique_ptr<ChunkCpuBuild> cpu;   // valid while Loading (worker writes it)
        TerrainData     data;                 // owns mesh/splat/heightField when Ready
        std::shared_ptr<TerrainObject> object;
        std::size_t     physHandle = static_cast<std::size_t>(-1);  // PhysicsWorld::TerrainHandle
        float           expireSeconds = 0.f;  // accumulates while Expiring
    };

    // GPU resources of an unloaded chunk, retired for a few frames so the GPU is no
    // longer referencing them before the splat descriptor is freed / mesh released.
    struct PendingUnload {
        TerrainData data;
        std::shared_ptr<TerrainObject> object;
        Texture retainedTex{};    // owns the splat GPU resource until release
        int splatSrvIdx = -1;     // bindless pool index to free (or -1)
        int framesLeft  = 0;
    };

    static int64_t packCoord(int col, int row) {
        return (static_cast<int64_t>(col) << 32) | static_cast<uint32_t>(static_cast<int32_t>(row));
    }

    mu::Vec3 worldOffset(int col, int row) const {
        return mu::Vec3(col * chunkSizeX_, 0.f, row * chunkSizeZ_);
    }

    const LoadedChunk* findReady(int col, int row) const;

    // Streaming helpers (Phase 5/6).
    std::unordered_set<int64_t> computeDesired(int curCol, int curRow) const;  // BFS hop<=maxHop_
    void startLoad(const ChunkIndexEntry& entry);     // enqueue worker-thread CPU build
    void drainCompletedBuilds();                      // finalize ready CPU builds (capped/frame)
    void unloadChunk(LoadedChunk& slot);              // physics off + retire GPU resources
    void tickGraveyard();                             // free retired GPU resources after delay

    // Activates a chunk's render object + physics collider (data already finalized).
    void activateChunkRenderAndPhysics(LoadedChunk& slot);

    GFX*          gfx_          = nullptr;
    PhysicsWorld* physicsWorld_ = nullptr;
    ThreadPool*   threadPool_   = nullptr;
    std::filesystem::path terrainDir_;

    TerrainLayerPalette palette_;
    ChunkIndex          index_;
    std::unordered_map<std::string, Texture>  texHashMap_;   // palette + per-chunk splats
    std::unordered_map<int64_t, LoadedChunk>  chunks_;
    std::unordered_map<int64_t, const ChunkIndexEntry*> indexByCoord_;
    std::vector<PendingUnload> graveyard_;

    float chunkSizeX_ = 0.f;   // uniform cell size (from the first index entry)
    float chunkSizeZ_ = 0.f;

    std::optional<std::pair<int,int>> lastCur_;   // last known player chunk

    // Streaming tunables.
    int   maxHop_                = 3;     // load chunks within this many neighbor hops
    float graceSeconds_          = 5.f;   // keep an out-of-range chunk this long before unload
    int   maxConcurrentLoads_    = 4;     // in-flight worker CPU builds
    int   maxGpuFinalizePerFrame_ = 2;    // GPU finalize (+brief wait) per frame
    int   graveyardFrameDelay_   = 4;     // frames to retain unloaded GPU resources
};

#endif // __terrainChunkManager_HPP
