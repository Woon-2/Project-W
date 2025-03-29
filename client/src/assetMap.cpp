#include "assetMap.hpp"
#include "resourcePath.hpp"

#include "enumUtil.hpp"

#include <vector>

const ModelInfo& assetModelInfo(AssetModel asset) {
    static auto sAssetModelInfos = std::vector<ModelInfo>{
        ModelInfo{
            .key = "GO_OH-58D",
            .path = resourcePath/"models/HelicopterModel/GO_OH-58D.bin"
        },
        ModelInfo{
            .key = "GO_URP_Tree_0",
            .path = resourcePath/"models/Trees/GO_URP_Tree_0.bin"
        },
        ModelInfo{
            .key = "GO_URP_Tree_1",
            .path = resourcePath/"models/Trees/GO_URP_Tree_1.bin"
        },
        ModelInfo{
            .key = "GO_URP_Tree_2",
            .path = resourcePath/"models/Trees/GO_URP_Tree_2.bin"
        }
    };

    return sAssetModelInfos[etoi(asset)];
}

const TextureInfo& assetTextureInfo(AssetTexture asset) {
    static auto sTextureInfos = std::vector<TextureInfo>{
        TextureInfo{
            .keys = {
                "GO_OH-58D_Default",
                "GO_OH-58D_Hellfire",
                "GO_OH-58D_Hydra",
                "GO_OH-58D_Texture"
            },
            .paths = {
                resourcePath/"models/HelicopterModel/Textures/Default.dds",
                resourcePath/"models/HelicopterModel/Textures/Hellfire.dds",
                resourcePath/"models/HelicopterModel/Textures/Hydra.dds",
                resourcePath/"models/HelicopterModel/Textures/Texture.dds"
            },
            .type = gfx::d3d12::TextureResource::Type::Texture
        },
        TextureInfo{
            .keys = {
                "GO_URP_Tree_0_Leaf",
                "GO_URP_Tree_0_Trunk"
            },
            .paths = {
                resourcePath/"models/Trees/Textures/URP_1_Leaf.dds",
                resourcePath/"models/Trees/Textures/URP_1_Trunk.dds",
            },
            .type = gfx::d3d12::TextureResource::Type::Texture
        },
        TextureInfo{
            .keys = {
                "GO_URP_Tree_1_Leaf",
                "GO_URP_Tree_1_Trunk"
            },
            .paths = {
                resourcePath/"models/Trees/Textures/URP_2_Leaf.dds",
                resourcePath/"models/Trees/Textures/URP_2_Trunk.dds",
            },
            .type = gfx::d3d12::TextureResource::Type::Texture
        },
        TextureInfo{
            .keys = {
                "GO_URP_Tree_2_Leaf",
                "GO_URP_Tree_2_Trunk"
            },
            .paths = {
                resourcePath/"models/Trees/Textures/URP_3_Leaf.dds",
                resourcePath/"models/Trees/Textures/URP_3_Trunk.dds",
            },
            .type = gfx::d3d12::TextureResource::Type::Texture
        },
        TextureInfo{
            .keys = {
                "Grass_A_BaseColor",
                "Grass_A_Normal"
            },
            .paths = {
                resourcePath/"terrains/Textures/Grass_A_BaseColor.dds",
                resourcePath/"terrains/Textures/Grass_A_Normal.dds",
            },
            .type = gfx::d3d12::TextureResource::Type::Texture
        },
        TextureInfo{
            .keys = {
                "Terrain_0_0_HeightMap",
                "Terrain_0_1_HeightMap",
                "Terrain_0_2_HeightMap",
                "Terrain_1_0_HeightMap",
                "Terrain_1_1_HeightMap",
                "Terrain_1_2_HeightMap",
                "Terrain_2_0_HeightMap",
                "Terrain_2_1_HeightMap",
                "Terrain_2_2_HeightMap",
            },
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
            },
            .type = gfx::d3d12::TextureResource::Type::Texture
        }
    };

    return sTextureInfos[etoi(asset)];
}

const BVHInfo& assetBVHInfo(AssetBVH asset) {
    static auto sAssetBVHInfos = std::vector<BVHInfo>{
        BVHInfo{
            .key = "GO_OH-58D",
            .path = resourcePath/"models/HelicopterModel/GO_OH-58D.bvh"
        },
        BVHInfo{
            .key = "Terrain_0_0",
            .path = resourcePath/"terrains/BVHs/Terrain_0_0.bvh"
        },
        BVHInfo{
            .key = "Terrain_0_1",
            .path = resourcePath/"terrains/BVHs/Terrain_0_1.bvh"
        },
        BVHInfo{
            .key = "Terrain_0_2",
            .path = resourcePath/"terrains/BVHs/Terrain_0_2.bvh"
        },
        BVHInfo{
            .key = "Terrain_1_0",
            .path = resourcePath/"terrains/BVHs/Terrain_1_0.bvh"
        },
        BVHInfo{
            .key = "Terrain_1_1",
            .path = resourcePath/"terrains/BVHs/Terrain_1_1.bvh"
        },
        BVHInfo{
            .key = "Terrain_1_2",
            .path = resourcePath/"terrains/BVHs/Terrain_1_2.bvh"
        },
        BVHInfo{
            .key = "Terrain_2_0",
            .path = resourcePath/"terrains/BVHs/Terrain_2_0.bvh"
        },
        BVHInfo{
            .key = "Terrain_2_1",
            .path = resourcePath/"terrains/BVHs/Terrain_2_1.bvh"
        },
        BVHInfo{
            .key = "Terrain_2_2",
            .path = resourcePath/"terrains/BVHs/Terrain_2_2.bvh"
        }
    };

    return sAssetBVHInfos[etoi(asset)];
}