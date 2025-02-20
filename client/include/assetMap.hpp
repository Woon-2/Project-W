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

enum class AssetTexture {
    Helicopter,
    Tree0,
    Tree1,
    Tree2,
    Terrain
};

struct ModelInfo {
    gfx::d3d12engine::Core::RefModelKey key;
    std::filesystem::path path;
};

struct TextureInfo {
    std::vector< gfx::d3d12engine::Core::TextureKey > keys;
    std::vector<std::filesystem::path> paths;
    gfx::d3d12::TextureResource::Type type;
};

const ModelInfo& assetModelInfo(AssetModel asset);
const TextureInfo& assetTextureInfo(AssetTexture asset);



#endif  // __AssetMap_HPP