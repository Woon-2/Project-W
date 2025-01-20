#include "assetMap.hpp"
#include "resourcePath.hpp"

#include "enumUtil.hpp"

#include <vector>

const ModelInfo& assetModelInfo(AssetModel asset) {
    static auto sAssetModelInfos = std::vector<ModelInfo>{
        ModelInfo{
            .id = "Helicopter",
            .path = resourcePath/"models/HelicopterModel/GO_OH-58D.bin"
        },
        ModelInfo{
            .id = "Tree0",
            .path = resourcePath/"models/Trees/GO_URP_Tree_0.bin"
        },
        ModelInfo{
            .id = "Tree1",
            .path = resourcePath/"models/Trees/GO_URP_Tree_1.bin"
        },
        ModelInfo{
            .id = "Tree2",
            .path = resourcePath/"models/Trees/GO_URP_Tree_2.bin"
        }
    };

    return sAssetModelInfos[etoi(asset)];
}

const TextureInfo& assetTextureInfo(AssetTexture asset) {
    static auto sTextureInfos = std::vector<TextureInfo>{
        TextureInfo{
            .paths = {
                resourcePath/"models/HelicopterModel/Textures/Default.dds",
                resourcePath/"models/HelicopterModel/Textures/Hellfire.dds",
                resourcePath/"models/HelicopterModel/Textures/Hydra.dds",
                resourcePath/"models/HelicopterModel/Textures/Texture.dds"
            },
            .type = gfx::d3d12::TextureResource::Type::Texture
        },
        TextureInfo{
            .paths = {
                resourcePath/"models/Trees/Textures/URP_1_Leaf.dds",
                resourcePath/"models/Trees/Textures/URP_1_Trunk.dds",
            },
            .type = gfx::d3d12::TextureResource::Type::Texture
        },
        TextureInfo{
            .paths = {
                resourcePath/"models/Trees/Textures/URP_2_Leaf.dds",
                resourcePath/"models/Trees/Textures/URP_2_Trunk.dds",
            },
            .type = gfx::d3d12::TextureResource::Type::Texture
        },
        TextureInfo{
            .paths = {
                resourcePath/"models/Trees/Textures/URP_3_Leaf.dds",
                resourcePath/"models/Trees/Textures/URP_3_Trunk.dds",
            },
            .type = gfx::d3d12::TextureResource::Type::Texture
        },
        TextureInfo{
            .paths = {
                resourcePath/"terrains/HeightMaps/Terrain_0_0_HeightMap.dds",
                resourcePath/"terrains/HeightMaps/Terrain_0_1_HeightMap.dds",
                resourcePath/"terrains/HeightMaps/Terrain_0_2_HeightMap.dds",
                resourcePath/"terrains/HeightMaps/Terrain_1_0_HeightMap.dds",
                resourcePath/"terrains/HeightMaps/Terrain_1_1_HeightMap.dds",
                resourcePath/"terrains/HeightMaps/Terrain_1_2_HeightMap.dds",
                resourcePath/"terrains/HeightMaps/Terrain_2_0_HeightMap.dds",
                resourcePath/"terrains/HeightMaps/Terrain_2_1_HeightMap.dds",
                resourcePath/"terrains/HeightMaps/Terrain_2_2_HeightMap.dds",

                resourcePath/"terrains/Textures/Grass_A_BaseColor.dds",
                resourcePath/"terrains/Textures/Grass_A_Normal.dds",
            },
            .type = gfx::d3d12::TextureResource::Type::Texture
        }
    };

    return sTextureInfos[etoi(asset)];
}
