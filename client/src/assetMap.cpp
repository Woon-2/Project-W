#include "assetMap.hpp"
#include "resourcePath.hpp"

#include "enumUtil.hpp"

#include <vector>

ModelInfo assetModelInfo(AssetModel asset) {
    static auto sAssetModelInfos = std::vector<ModelInfo>{
        ModelInfo{
            .id = "Helicopter",
            .path = resourcePath/"models/HelicopterModel/OH-58D.bin"
        }
    };

    return sAssetModelInfos[etoi(asset)];
}

std::vector<TextureInfo> assetTextureInfo(AssetTexture asset) {
    static auto sTextureInfos = std::vector<std::vector<TextureInfo>>{
        std::vector<TextureInfo> {
            TextureInfo{
                .paths = {
                    resourcePath/"models/HelicopterModel/Textures/Default.dds",
                    resourcePath/"models/HelicopterModel/Textures/Hellfire.dds",
                    resourcePath/"models/HelicopterModel/Textures/Hydra.dds",
                    resourcePath/"models/HelicopterModel/Textures/Texture.dds"
                },
                .type = gfx::d3d12::TextureResource::Type::Texture
            }
        }
    };

    return sTextureInfos[etoi(asset)];
}
