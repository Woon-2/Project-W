#ifndef __AssetMap_HPP
#define __AssetMap_HPP

#include <filesystem>
#include <vector>

using AssetID = std::string;

enum class AssetModel {
    Helicopter,
    Character,
    Goblin,
    Tree0,
    Tree1,
    Tree2
};

enum class AssetBVH {
    Helicopter,
    Character,
    Goblin,
    Tree0,
    Tree1,
    Tree2,
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
    Goblin,
    Tree0,
    Tree1,
    Tree2,
    Terrain,
    TerrainHeightmap,
    SkySphere,
};

struct ModelInfo {
    AssetID key;
    std::filesystem::path geometryPath;
    std::filesystem::path animationPath;
};

struct TextureInfo {
    enum class Type {
        Texture,
        TextureArray,
        TextureCube
    };

    std::vector<AssetID> keys;
    std::vector<std::filesystem::path> paths;
    Type type;
};

struct BVHInfo {
    AssetID key;
    std::filesystem::path path;
};

const ModelInfo& assetModelInfo(AssetModel asset);
const TextureInfo& assetTextureInfo(AssetTexture asset);
const BVHInfo& assetBVHInfo(AssetBVH asset);



#endif  // __AssetMap_HPP