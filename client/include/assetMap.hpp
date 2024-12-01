#ifndef __AssetMap_HPP
#define __AssetMap_HPP

#include "d3d12engine/d3d12Engine.hpp"

#include <filesystem>
#include <vector>

enum class AssetModel {
    Helicopter
};

enum class AssetTexture {
    Helicopter
};

struct ModelInfo {
    gfx::d3d12::RefModelStorage::ID id;
    std::filesystem::path path;
};

struct TextureInfo {
    std::vector<std::filesystem::path> paths;
    gfx::d3d12::TextureResource::Type type;
};

ModelInfo assetModelInfo(AssetModel asset);
std::vector<TextureInfo> assetTextureInfo(AssetTexture asset);



#endif  // __AssetMap_HPP