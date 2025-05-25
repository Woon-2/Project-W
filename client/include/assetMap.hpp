#ifndef __AssetMap_HPP
#define __AssetMap_HPP

#include "d3d12engine/d3d12Engine.hpp"

#include <filesystem>
#include <vector>

enum class AssetModel {
    Helicopter,
    Character,
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
    Character,
    Tree0,
    Tree1,
    Tree2,
    Terrain,
    TerrainHeightmap,
    SkySphere,
};

struct ModelInfo {
    gfx::d3d12::ResourceStorage::ResID key;
    std::filesystem::path geometryPath;
    std::filesystem::path animationPath;
};

struct TextureInfo {
    std::vector<gfx::d3d12::ResourceStorage::ResID> keys;
    std::vector<std::filesystem::path> paths;
    gfx::d3d12::TextureResource::Type type;
};

struct BVHInfo {
    gfx::d3d12::ResourceStorage::ResID key;
    std::filesystem::path path;
};

const ModelInfo& assetModelInfo(AssetModel asset);
const TextureInfo& assetTextureInfo(AssetTexture asset);
const BVHInfo& assetBVHInfo(AssetBVH asset);



#endif  // __AssetMap_HPP