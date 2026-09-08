#ifndef room_server_terrain_hpp
#define room_server_terrain_hpp

#include "strongholdDef.hpp"
#include "collision.hpp"          // BVH (prop collision volumes)
#include "../common/zoneDef.hpp"
#include "../common/markerDef.hpp"
#include <filesystem>
#include <vector>
#include <string>
#include <unordered_map>
#include <optional>
#include <utility>
#include <cstdint>

// CPU-side height field data used for physics collision queries.
// Built from height.raw during loadChunkHeightField().
struct TerrainHeightField {
    int   resolution = 0;
    float sizeX      = 0.f;
    float sizeY      = 0.f;
    float sizeZ      = 0.f;
    std::vector<float> heights; // normalized [0,1], row-major: heights[y*N + x]

    bool empty() const { return heights.empty(); }

    // Bilinear interpolation. localX in [0,sizeX], localZ in [0,sizeZ].
    // Returns world-space Y (already scaled by sizeY).
    float getHeightAt(float localX, float localZ) const;

    // Central-difference surface normal at (localX, localZ).
    // Always returns a vector with positive Y (upward-facing).
    mu::Vec3 getNormalAt(float localX, float localZ) const;
};

// ---------------------------------------------------------------------------
// Chunk index (mirror of the client terrain chunk format).
// The server consumes only the per-chunk height data; render-only palette and
// splat fields are parsed to keep the stream aligned, then discarded.
// ---------------------------------------------------------------------------

// One scattered prop instance (server: collision authority for monster-vs-prop).
// Render-only fields (billboards, tint) are discarded during parsing.
struct ScatterInstance {
    uint16_t  protoIdx = 0;
    mu::Vec3  posLocal{};       // chunk-local position (ground-snapped at collider build)
    mu::NQuat rot{};            // orientation (trees yaw, details ground-align)
    float     scaleW = 1.f;     // XZ scale
    float     scaleH = 1.f;     // Y scale
};

struct ChunkIndexEntry {
    int   col = 0, row = 0;
    float sizeX = 0.f, sizeY = 0.f, sizeZ = 0.f;
    int   resolution = 0;
    int   alphamapResolution = 0;
    std::vector<std::pair<int, int>> neighbors;
    std::string heightPath;
    std::string splatPath; // parsed but unused on the server
    std::vector<ScatterInstance> scatter;   // per-chunk prop instances (collision)
};

struct ChunkIndex {
    int version = 0;
    std::vector<ChunkIndexEntry> chunks;
    std::vector<std::string>     scatterPrototypeNames;  // protoIdx -> prop model name
    std::vector<StrongholdDef>   strongholds;   // gameplay: monster spawner bases
    std::vector<ZoneDef>         zones;         // gameplay/cosmetic: trigger volumes
    std::vector<MarkerDef>       markers;       // generic placements (type+name+transform)
    // NOTE: the client also carries a shared TerrainLayerPalette here; the
    // server omits it (no rendering) but still parses its tags in parseChunkIndex.
};

// Parses chunks_index.bin. Reads (and discards) the shared palette descriptor
// to keep the binary stream aligned, then fills the per-chunk records.
ChunkIndex parseChunkIndex(const std::filesystem::path& terrainDir);

// Loads only the CPU-side height field for one chunk from its height.raw.
// No GPU resources are allocated.
TerrainHeightField loadChunkHeightField(const ChunkIndexEntry& entry,
                                        const std::filesystem::path& terrainDir);

// ---------------------------------------------------------------------------
// TerrainChunkManager (server: load-all, shared, no streaming)
//
// Mirrors the client class name and world-routing API, but the server variant
// loads EVERY chunk's height field once at boot and holds them read-only so a
// single instance can be shared across all rooms. There is no streaming,
// GPU, or rendering here (the client's update/submitDrawEvents/graveyard are
// intentionally absent). Per-room terrain colliders reference these shared
// height fields by const pointer.
// ---------------------------------------------------------------------------
class TerrainChunkManager {
public:
    // Parses chunks_index.bin and loads all chunk height fields once.
    void init(const std::filesystem::path& terrainDir);

    float    MU_CALLCONV heightAtWorld(float x, float z) const;
    mu::Vec3 MU_CALLCONV normalAtWorld(float x, float z) const;

    std::optional<std::pair<int, int>> chunkCoordAtWorld(float x, float z) const;

    bool empty() const { return index_.chunks.empty(); }

    std::size_t chunkCount() const { return index_.chunks.size(); }

    // Stronghold definitions parsed from chunks_index.bin (gameplay data).
    const std::vector<StrongholdDef>& strongholds() const { return index_.strongholds; }

    // Zone (trigger volume) definitions parsed from chunks_index.bin.
    const std::vector<ZoneDef>& zones() const { return index_.zones; }

    // Generic placement markers parsed from chunks_index.bin.
    const std::vector<MarkerDef>& markers() const { return index_.markers; }

    // Invokes fn(col, row, worldOffset, const TerrainHeightField*) for every
    // loaded chunk. Used by Room to register per-chunk terrain colliders.
    template <class F>
    void forEachChunk(F&& fn) const {
        for (const auto& e : index_.chunks) {
            const TerrainHeightField* hf = findHf(e.col, e.row);
            if (!hf) continue;
            fn(e.col, e.row, worldOffset(e.col, e.row), hf);
        }
    }

    mu::Vec3 worldOffset(int col, int row) const {
        return mu::Vec3(col * chunkSizeX_, 0.f, row * chunkSizeZ_);
    }

    // Builds + registers per-chunk collidable scatter (prop) colliders into the
    // given room PhysicsWorld. Call once per Room::init after terrain colliders.
    // Prop BVHs + instance data are shared (loaded in init); each room bakes its
    // own colliders (mirrors the per-room terrain colliders).
    void registerScatterColliders(class PhysicsWorld& physicsWorld) const;

private:
    static int64_t packCoord(int col, int row) {
        return (static_cast<int64_t>(col) << 32) |
               static_cast<uint32_t>(static_cast<int32_t>(row));
    }

    const TerrainHeightField* findHf(int col, int row) const {
        auto it = heightFields_.find(packCoord(col, row));
        return (it != heightFields_.end()) ? &it->second : nullptr;
    }

    // Loads model-space BVHs for collidable scatter prototypes from
    // "<propDir>/<name>Server.bin" (only those that have a server BV export).
    void loadPropBVHs();

    // One raw scatter instance resolved to world primitives (ground-snapped pos,
    // orientation, per-axis scale). Mirrors the client resolveInstanceXform so
    // both peers build identical prop geometry.
    struct ScatterWorldXform {
        mu::Vec3  pos{};
        mu::NQuat rot{};
        mu::Vec3  scale{ 1.f, 1.f, 1.f };
    };
    ScatterWorldXform resolveInstanceXform(int col, int row,
                                           const TerrainHeightField* hf,
                                           const ScatterInstance& inst) const;

    ChunkIndex index_;
    std::unordered_map<int64_t, TerrainHeightField> heightFields_;
    std::unordered_map<std::string, BVH>            propBVHs_;   // prop name -> model-space BVH
    float chunkSizeX_ = 0.f;
    float chunkSizeZ_ = 0.f;
};

#endif // room_server_terrain_hpp
