#ifndef __AssetMap_HPP
#define __AssetMap_HPP

#include "d3d12engine/d3d12Engine.hpp"

#include <filesystem>
#include <vector>

enum class AssetModel {
    Helicopter,
    Tree0,
    Tree1,
    Tree2
};

enum class AssetBVH {
    Helicopter,
    Terrain_0_0,
    Terrain_0_1,
    Terrain_0_2,
    Terrain_1_0,
    Terrain_1_1,
    Terrain_1_2,
    Terrain_2_0,
    Terrain_2_1,
    Terrain_2_2
};

enum class AssetTexture {
    Helicopter,
    Tree0,
    Tree1,
    Tree2,
    Terrain,
    TerrainHeightmap,
};

struct ModelInfo {
    gfx::d3d12engine::Core::RefModelKey key;
    std::filesystem::path path;
};

struct TextureInfo {
    std::vector<gfx::d3d12engine::Core::TextureKey> keys;
    std::vector<std::filesystem::path> paths;
    gfx::d3d12::TextureResource::Type type;
};

struct BVHInfo {
    gfx::d3d12engine::Core::BVHPathKey key;
    std::filesystem::path path;
};

const ModelInfo& assetModelInfo(AssetModel asset);
const TextureInfo& assetTextureInfo(AssetTexture asset);
const BVHInfo& assetBVHInfo(AssetBVH asset);



#endif  // __AssetMap_HPP