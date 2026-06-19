#ifndef __terrainChunkManager_HPP
#define __terrainChunkManager_HPP

#include "terrain.hpp"
#include "object.hpp"      // TerrainObject
#include "frustumCull.hpp" // per-instance scatter VFC

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
class Light;

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
    // `light` drives shadow (light-frustum) culling for chunks and BVH scatter props,
    // so off-camera casters still render into the shadow map. (Distinct from the main
    // camera frustum set via setCullCamera(), which only gates the gbuffer/Hi-Z path.)
    void submitDrawEvents(GFX& gfx, const Light& light);

    // Camera frustum + eye position for per-instance scatter culling. Call once per frame
    // before submitDrawEvents(). The frustum drives BVH-prop view-frustum culling; the eye
    // position selects near-camera BVH props as Hi-Z occluders. (Distinct from the player
    // position passed to update(), which anchors foliage distance culling.)
    void setCullCamera(const Frustum& frustum, mu::Vec3 eyePos) {
        frustum_     = frustum;
        cameraPos_   = eyePos;
        hasFrustum_  = true;
    }

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

    // ---- scatter (Unity Paint Tree / Paint Detail) ----

    // One render part of a resolved scatter prototype (a tree model may have several
    // meshes/submeshes; a billboard has exactly one).
    struct ScatterDrawPart {
        const Mesh*     mesh     = nullptr;
        const SubMesh*  subMesh  = nullptr;
        const Material* material = nullptr;
        mu::Mat4x4      meshXform;            // model dress transform (identity for billboards)
    };

    // A scatter prototype resolved to renderable parts (+ optional collision model).
    struct ResolvedScatterProto {
        std::vector<ScatterDrawPart> parts;
        AABB         localBounds{};           // union of part mesh bounds (model space)
        const Model* model = nullptr;         // tree/mesh-detail model (model-space BVH); null for billboards
        bool         collidable = false;      // model && !model->bvh.empty()
    };

    // A resident scattered instance on a loaded chunk. Kept (NOT emit-and-forget) so a
    // future ScatterCollider can transform the prototype's model-space BVH to world space.
    struct ScatterInstanceResolved {
        u16t       protoIdx = 0;
        mu::Mat4x4 world;                     // worldOffset + posLocal, yaw, scale [+ billboard size]
        AABB       worldAABB{};               // culling + future collision broadphase
    };

    struct LoadedChunk {
        ChunkIndexEntry entry;
        State           state = State::Unloaded;
        std::unique_ptr<ChunkCpuBuild> cpu;   // valid while Loading (worker writes it)
        TerrainData     data;                 // owns mesh/splat/heightField when Ready
        std::shared_ptr<TerrainObject> object;
        std::size_t     physHandle = static_cast<std::size_t>(-1);  // PhysicsWorld::TerrainHandle
        std::size_t     scatterHandle = static_cast<std::size_t>(-1);  // PhysicsWorld::ScatterHandle
        float           expireSeconds = 0.f;  // accumulates while Expiring
        std::vector<ScatterInstanceResolved> scatter;  // resident scattered foliage instances
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

    // Scatter: loads prop models + billboard assets and resolves the prototype table
    // (called once after the palette in init); resolves a chunk's instances on activate;
    // and submits per-instance draw events (auto-instanced by the PBR pipelines).
    void loadScatterAssets();
    void resolveChunkScatter(LoadedChunk& slot);
    void submitScatterDrawEvents(GFX& gfx, const LoadedChunk& slot, const Light& light) const;

    // One raw scatter instance resolved to world primitives (ground-snapped pos,
    // orientation, per-axis scale). Shared by render-instance resolution and the
    // static prop collider so both reference identical geometry.
    struct ScatterWorldXform {
        mu::Vec3  pos{};
        mu::NQuat rot{};
        mu::Vec3  scale{ 1.f, 1.f, 1.f };
    };
    ScatterWorldXform resolveInstanceXform(const LoadedChunk& slot, const ScatterInstance& inst) const;

    // Builds + registers a per-chunk ScatterCollider from collidable instances.
    void registerChunkScatterCollider(LoadedChunk& slot);

    GFX*          gfx_          = nullptr;
    PhysicsWorld* physicsWorld_ = nullptr;
    ThreadPool*   threadPool_   = nullptr;
    std::filesystem::path terrainDir_;

    TerrainLayerPalette palette_;
    ChunkIndex          index_;
    std::unordered_map<std::string, Texture>  texHashMap_;   // palette + per-chunk splats + billboard textures

    // Scatter assets (loaded once after the palette). propModels_ is environment-only
    // (separate from AssetManager's per-character explicit members); keyed by prototype Name.
    std::unordered_map<std::string, Model> propModels_;
    std::vector<Material>                  billboardMaterials_;  // one per BillboardGrass prototype
    Mesh                                   billboardMesh_;       // shared unit cross-quad (two-sided)
    std::vector<ResolvedScatterProto>      resolvedProtos_;      // parallel to index_.scatterPrototypes
    static constexpr float                 kFoliageAlphaCutoff = 0.33f;

    // Detail draw-distance cull (Unity's "Detail Distance" analogue): details/grass are
    // only emitted within this radius of the player; trees are always emitted (landmarks).
    // Bounds per-frame DrawEvent count so the instancing buffers never overflow.
    static constexpr float                 kDetailCullRadius = 80.f;
    mu::Vec3                               cullCenter_{};
    bool                                   hasCullCenter_ = false;

    // Per-instance VFC (BVH props) + Hi-Z occluder selection state, set by setCullCamera().
    // Near-camera BVH props (within kPropOccluderRadius of the camera eye) are submitted as
    // Hi-Z occluders so they occlude farther BVH props (and characters) behind them.
    static constexpr float                 kPropOccluderRadius = 40.f;
    Frustum                                frustum_{};
    mu::Vec3                               cameraPos_{};
    bool                                   hasFrustum_ = false;

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
