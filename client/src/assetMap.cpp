#include "assetMap.hpp"
#include "resourcePath.hpp"

#include "enumUtil.hpp"

#include <vector>

gfx::d3d12::RefModelStorage::ID assetModelID(AssetModel asset) {
    static auto sAssetModelIDs = std::vector<gfx::d3d12::RefModelStorage::ID>{
        "helicopter"
    };

    return sAssetModelIDs[etoi(asset)];
}

std::vector<std::filesystem::path> assetTextureID(AssetTexture asset) {
    static auto sAssetTextureIDs = std::vector<std::vector<std::filesystem::path>>{
        std::vector<std::filesystem::path>{
            resourcePath/"models/HelicopterModel/Textures/Default.dds",
            resourcePath/"models/HelicopterModel/Textures/Hellfire.dds",
            resourcePath/"models/HelicopterModel/Textures/Hydra.dds",
            resourcePath/"models/HelicopterModel/Textures/Texture.dds"
        }
    };

    return sAssetTextureIDs[etoi(asset)];
}
