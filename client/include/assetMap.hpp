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

gfx::d3d12::RefModelStorage::ID assetModelID(AssetModel asset);
std::vector<std::filesystem::path> assetTextureID(AssetTexture asset);



#endif  // __AssetMap_HPP