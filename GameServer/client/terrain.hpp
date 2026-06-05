#ifndef __terrain_HPP
#define __terrain_HPP

#include "gfxUtil.hpp"
#include "mesh.hpp"
#include "mathUtil.hpp"
#include "../common/zoneDef.hpp"
#include "../common/markerDef.hpp"

#include <atomic>

// CPU-side height field data used for physics collision queries.
// Built alongside the GPU mesh during loadTerrainFromFiles().
struct TerrainHeightField {
	int   resolution = 0;                // N: grid is N x N vertices
	float sizeX      = 0.f;
	float sizeY      = 0.f;
	float sizeZ      = 0.f;
	std::vector<float> heights;          // normalized [0,1], row-major: heights[y*N + x]

	bool empty() const { return heights.empty(); }

	// Bilinear interpolation. localX in [0,sizeX], localZ in [0,sizeZ].
	// Returns world-space Y (i.e., already scaled by sizeY).
	float getHeightAt(float localX, float localZ) const;

	// Central-difference surface normal at (localX, localZ).
	// Always returns a vector with positive Y (upward-facing).
	mu::Vec3 getNormalAt(float localX, float localZ) const;
};

// One terrain layer: diffuse + normal map textures, tiling parameters, and PBR scalars.
struct TerrainLayer {
    Texture diffuse;
    Texture normalMap;
    float tileSizeX   = 1.f;
    float tileSizeY   = 1.f;
    float tileOffsetX = 0.f;
    float tileOffsetY = 0.f;
    float metallic    = 0.f;
    float roughness   = 0.85f;  // default matches former hard-coded value
};

// All data required to render a single terrain chunk.
// Per chunk: splatMap + mesh + heightField are owned; layers are filled by copying
// the shared TerrainLayerPalette's BindlessIndex handles (one GPU texture set, shared).
struct TerrainData {
    int   heightmapResolution = 0;  // N: grid is N x N vertices
    int   alphamapResolution  = 0;
    float sizeX = 0.f;
    float sizeY = 0.f;
    float sizeZ = 0.f;
    int   layerCount = 0;

    int   chunkCol = 0;             // absolute grid coord (signed, arbitrary range)
    int   chunkRow = 0;

    std::vector<TerrainLayer> layers;
    Texture splatMap;   // RGBA splat map: channel per layer blend weight

    Mesh mesh;          // GPU grid mesh built from height.raw

	TerrainHeightField heightField; // CPU-side height data for physics collision
};

// ---------------------------------------------------------------------------
// Chunk streaming: shared palette, global index, CPU build product
// ---------------------------------------------------------------------------

// Unified layer palette shared by all chunks. Loaded once (single-threaded) by
// the ChunkManager; chunks copy these layer handles into TerrainData::layers.
struct TerrainLayerPalette {
    int layerCount = 0;
    std::vector<TerrainLayer> layers;          // diffuse/normal handles + scalars (filled by loadLayerPalette)
    std::vector<std::string>  diffusePaths;    // load-time only (from index); textures loaded into layers
    std::vector<std::string>  normalPaths;     // load-time only
    bool loaded = false;
};

// One chunk's metadata as parsed from chunks_index.bin (no heavy geometry).
struct ChunkIndexEntry {
    int   col = 0, row = 0;
    float sizeX = 0.f, sizeY = 0.f, sizeZ = 0.f;
    int   resolution = 0;
    int   alphamapResolution = 0;
    std::vector<std::pair<int,int>> neighbors;   // 4-neighbor (col,row), existing only
    std::string heightPath;
    std::string splatPath;
};

// Parsed global index: shared palette descriptor + all chunk records.
struct ChunkIndex {
    int version = 0;
    TerrainLayerPalette palette;        // path/scalar info; textures loaded separately
    std::vector<ChunkIndexEntry> chunks;
    std::vector<ZoneDef> zones;         // trigger volumes (client uses local cosmetic zones)
    std::vector<MarkerDef> markers;     // generic placements (type+name+transform)
};

// CPU-only product of a chunk build job. Contains NO GPU resources, so it can be
// produced on a worker thread without touching DescriptorPool / texHashMap / cmdList.
struct ChunkCpuBuild {
    int   col = 0, row = 0;
    float sizeX = 0.f, sizeY = 0.f, sizeZ = 0.f;
    int   resolution = 0;
    int   alphamapResolution = 0;
    std::string splatPath;

    std::vector<XMFLOAT3> positions, normals, tangents, bitangents;
    std::vector<XMFLOAT2> uvs;
    std::vector<u32t>     indices;
    std::vector<float>    normalizedHeights;   // [0,1], row-major, for TerrainHeightField

    std::atomic<bool>     done{false};         // set true by the worker when finished
    bool                  failed = false;
};

// ---------------------------------------------------------------------------
// Chunk streaming API  (terrain is loaded per-chunk; see TerrainChunkManager)
// ---------------------------------------------------------------------------

// Parses the global index file (chunks_index.bin). Paths are resolved relative
// to terrainDir when not found as-is. Fills palette descriptor + chunk records.
ChunkIndex parseChunkIndex(const std::filesystem::path& terrainDir);

// Loads the shared layer palette textures into the SRV pool (MAIN THREAD ONLY:
// touches DescriptorPool + texHashMap). Call once before streaming chunks.
TerrainLayerPalette loadLayerPalette(
    const ChunkIndex& index,
    const std::filesystem::path& terrainDir,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    std::unordered_map<std::string, Texture>& texHashMap,
    DescriptorPool& texPool,
    Fence& fenceToAssociate
);

// Builds a chunk's CPU-side mesh arrays + normalized height field from its
// height.raw. PURE CPU — safe to run on a worker thread. No D3D calls.
void buildChunkCpu(
    const ChunkIndexEntry& entry,
    const std::filesystem::path& terrainDir,
    ChunkCpuBuild& out
);

// Creates GPU resources from a finished ChunkCpuBuild and assembles a TerrainData.
// MAIN THREAD ONLY: creates buffers, records copies into cmdList, loads the splat
// texture (DescriptorPool + texHashMap), and copies shared palette layer handles.
TerrainData finalizeChunkGpu(
    const ChunkCpuBuild& cpu,
    const TerrainLayerPalette& palette,
    const std::filesystem::path& terrainDir,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    std::unordered_map<std::string, Texture>& texHashMap,
    DescriptorPool& texPool,
    Fence& fenceToAssociate
);

#endif // __terrain_HPP
