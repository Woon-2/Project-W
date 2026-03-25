#ifndef __terrain_HPP
#define __terrain_HPP

#include "gfxUtil.hpp"
#include "mesh.hpp"

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

// All data required to render a Unity-exported terrain.
struct TerrainData {
    int   heightmapResolution = 0;  // N: grid is N x N vertices
    int   alphamapResolution  = 0;
    float sizeX = 0.f;
    float sizeY = 0.f;
    float sizeZ = 0.f;
    int   layerCount = 0;

    std::vector<TerrainLayer> layers;
    Texture splatMap;   // RGBA splat map: channel per layer blend weight

    Mesh mesh;          // GPU grid mesh built from height.raw
};

// Loads terrain from resources/terrains directory.
//   - Parses terrain_manifest.bin (tag-based) for file paths.
//   - Parses terrain_meta.bin (raw binary) for dimensions and layer info.
//   - Reads height.raw and builds a GPU grid mesh.
//   - Loads splat map and per-layer textures into the SRV pool.
// Upload buffers are associated with fenceToAssociate; wait on that fence
// before releasing any resources that were passed to this function.
TerrainData loadTerrainFromFiles(
    const std::filesystem::path& terrainDir,
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmdList,
    std::unordered_map<std::string, Texture>& texHashMap,
    DescriptorPool& texPool,
    Fence& fenceToAssociate
);

#endif // __terrain_HPP
