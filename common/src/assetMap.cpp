#include "assetMap.hpp"
#include "resourcePath.hpp"

#include "enumUtil.hpp"

#include <vector>

const ModelInfo& assetModelInfo(AssetModel asset) {
    static auto sAssetModelInfos = std::vector<ModelInfo>{
        ModelInfo{
            .key = "GO_OH-58D",
            .geometryPath = getResourcePath()/"models\\HelicopterModel\\GO_OH-58D.bin"
        },
        ModelInfo{
            .key = "GO_Character",
            .geometryPath = getResourcePath()/"models\\Character\\GO_Character.bin",
            .animationPath = getResourcePath()/"models\\Character\\GO_Character.anim"
        },
        ModelInfo{
            .key = "GO_Goblin",
            .geometryPath = getResourcePath()/"models\\Goblin\\GO_Goblin.bin",
            .animationPath = getResourcePath()/"models\\Goblin\\GO_Goblin.anim"
        },
        ModelInfo{
            .key = "GO_URP_Tree_0",
            .geometryPath = getResourcePath()/"models\\Trees\\GO_URP_Tree_0.bin"
        },
        ModelInfo{
            .key = "GO_URP_Tree_1",
            .geometryPath = getResourcePath()/"models\\Trees\\GO_URP_Tree_1.bin"
        },
        ModelInfo{
            .key = "GO_URP_Tree_2",
            .geometryPath = getResourcePath()/"models\\Trees\\GO_URP_Tree_2.bin"
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
                getResourcePath()/"models\\HelicopterModel\\Textures\\Default.dds",
                getResourcePath()/"models\\HelicopterModel\\Textures\\Hellfire.dds",
                getResourcePath()/"models\\HelicopterModel\\Textures\\Hydra.dds",
                getResourcePath()/"models\\HelicopterModel\\Textures\\Texture.dds"
            },
            .type = TextureInfo::Type::Texture
        },
        TextureInfo{
            .keys = {
                "T_Belt_Peasant_MR",
                "T_Belt_Peasant_N",
                "T_Belt_Peasant_Rd_D",
                "T_Cape_Peasant_Bl_D",
                "T_Cape_Peasant_MR",
                "T_Cape_Peasant_N",
                "T_Gloves_Peasant_Bl_D",
                "T_Gloves_Peasant_MR",
                "T_Gloves_Peasant_N",
                "T_HU_Eye_Gn_D",
                "T_HU_Eye_MR",
                "T_HU_Eye_N",
                "T_HU_F_Body_05_D",
                "T_HU_F_Body_MR",
                "T_HU_F_Body_N",
                "T_HU_F_Body_Preview",
                "T_HU_F_Facial_01_Bk_D",
                "T_HU_F_Facial_01_MR",
                "T_HU_F_Facial_01_N",
                "T_HU_F_Head_05_A_D",
                "T_HU_F_Head_A_MR",
                "T_HU_F_Head_A_N",
                "T_HU_F_Head_UH_Bk_D",
                "T_HU_Hair_01_Gr_D",
                "T_HU_Hair_01_MR",
                "T_HU_Hair_01_N",
                "T_Set_Peasant_Bl_D",
                "T_Set_Peasant_MR",
                "T_Set_Peasant_N",
                "T_Set_Peasant_Rd_D",
            },
            .paths = {
                getResourcePath()/"models\\Character\\Textures\\T_Belt_Peasant_MR.dds",
                getResourcePath()/"models\\Character\\Textures\\T_Belt_Peasant_N.dds",
                getResourcePath()/"models\\Character\\Textures\\T_Belt_Peasant_Rd_D.dds",
                getResourcePath()/"models\\Character\\Textures\\T_Cape_Peasant_Bl_D.dds",
                getResourcePath()/"models\\Character\\Textures\\T_Cape_Peasant_MR.dds",
                getResourcePath()/"models\\Character\\Textures\\T_Cape_Peasant_N.dds",
                getResourcePath()/"models\\Character\\Textures\\T_Gloves_Peasant_Bl_D.dds",
                getResourcePath()/"models\\Character\\Textures\\T_Gloves_Peasant_MR.dds",
                getResourcePath()/"models\\Character\\Textures\\T_Gloves_Peasant_N.dds",
                getResourcePath()/"models\\Character\\Textures\\T_HU_Eye_Gn_D.dds",
                getResourcePath()/"models\\Character\\Textures\\T_HU_Eye_MR.dds",
                getResourcePath()/"models\\Character\\Textures\\T_HU_Eye_N.dds",
                getResourcePath()/"models\\Character\\Textures\\T_HU_F_Body_05_D.dds",
                getResourcePath()/"models\\Character\\Textures\\T_HU_F_Body_MR.dds",
                getResourcePath()/"models\\Character\\Textures\\T_HU_F_Body_N.dds",
                getResourcePath()/"models\\Character\\Textures\\T_HU_F_Body_Preview.dds",
                getResourcePath()/"models\\Character\\Textures\\T_HU_F_Facial_01_Bk_D.dds",
                getResourcePath()/"models\\Character\\Textures\\T_HU_F_Facial_01_MR.dds",
                getResourcePath()/"models\\Character\\Textures\\T_HU_F_Facial_01_N.dds",
                getResourcePath()/"models\\Character\\Textures\\T_HU_F_Head_05_A_D.dds",
                getResourcePath()/"models\\Character\\Textures\\T_HU_F_Head_A_MR.dds",
                getResourcePath()/"models\\Character\\Textures\\T_HU_F_Head_A_N.dds",
                getResourcePath()/"models\\Character\\Textures\\T_HU_F_Head_UH_Bk_D.dds",
                getResourcePath()/"models\\Character\\Textures\\T_HU_Hair_01_Gr_D.dds",
                getResourcePath()/"models\\Character\\Textures\\T_HU_Hair_01_MR.dds",
                getResourcePath()/"models\\Character\\Textures\\T_HU_Hair_01_N.dds",
                getResourcePath()/"models\\Character\\Textures\\T_Set_Peasant_Bl_D.dds",
                getResourcePath()/"models\\Character\\Textures\\T_Set_Peasant_MR.dds",
                getResourcePath()/"models\\Character\\Textures\\T_Set_Peasant_N.dds",
                getResourcePath()/"models\\Character\\Textures\\T_Set_Peasant_Rd_D.dds",
            },
            .type = TextureInfo::Type::Texture
        },
        TextureInfo{
            .keys = {
                "GO_Goblin_ColorSet",
            },
            .paths = {
                getResourcePath()/"models\\Goblin\\Textures\\Color_Set_1.dds",
            },
            .type = TextureInfo::Type::Texture
        },
        TextureInfo{
            .keys = {
                "GO_URP_Tree_0_Leaf",
                "GO_URP_Tree_0_Trunk"
            },
            .paths = {
                getResourcePath()/"models\\Trees\\Textures\\URP_1_Leaf.dds",
                getResourcePath()/"models\\Trees\\Textures\\URP_1_Trunk.dds",
            },
            .type = TextureInfo::Type::Texture
        },
        TextureInfo{
            .keys = {
                "GO_URP_Tree_1_Leaf",
                "GO_URP_Tree_1_Trunk"
            },
            .paths = {
                getResourcePath()/"models\\Trees\\Textures\\URP_2_Leaf.dds",
                getResourcePath()/"models\\Trees\\Textures\\URP_2_Trunk.dds",
            },
            .type = TextureInfo::Type::Texture
        },
        TextureInfo{
            .keys = {
                "GO_URP_Tree_2_Leaf",
                "GO_URP_Tree_2_Trunk"
            },
            .paths = {
                getResourcePath()/"models\\Trees\\Textures\\URP_3_Leaf.dds",
                getResourcePath()/"models\\Trees\\Textures\\URP_3_Trunk.dds",
            },
            .type = TextureInfo::Type::Texture
        },
        TextureInfo{
            .keys = {
                "Grass_A_BaseColor",
                "Grass_A_Normal"
            },
            .paths = {
                getResourcePath()/"terrains\\Textures\\Grass_A_BaseColor.dds",
                getResourcePath()/"terrains\\Textures\\Grass_A_Normal.dds",
            },
            .type = TextureInfo::Type::Texture
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
                getResourcePath()/"terrains\\HeightMaps\\Terrain_0_0_HeightMap.dds",
                getResourcePath()/"terrains\\HeightMaps\\Terrain_0_1_HeightMap.dds",
                getResourcePath()/"terrains\\HeightMaps\\Terrain_0_2_HeightMap.dds",
                getResourcePath()/"terrains\\HeightMaps\\Terrain_1_0_HeightMap.dds",
                getResourcePath()/"terrains\\HeightMaps\\Terrain_1_1_HeightMap.dds",
                getResourcePath()/"terrains\\HeightMaps\\Terrain_1_2_HeightMap.dds",
                getResourcePath()/"terrains\\HeightMaps\\Terrain_2_0_HeightMap.dds",
                getResourcePath()/"terrains\\HeightMaps\\Terrain_2_1_HeightMap.dds",
                getResourcePath()/"terrains\\HeightMaps\\Terrain_2_2_HeightMap.dds",
            },
            .type = TextureInfo::Type::Texture
		},
		TextureInfo{
			.keys = {
				"SkySphere"
			},
			.paths = {
				getResourcePath() / "models\\SkySphere\\Textures\\Sky02.dds",
			},
			.type = TextureInfo::Type::Texture
		}
    };

    return sTextureInfos[etoi(asset)];
}

const BVHInfo& assetBVHInfo(AssetBVH asset) {
    static auto sAssetBVHInfos = std::vector<BVHInfo>{
        BVHInfo{
            .key = "GO_OH-58D",
            .path = getResourcePath()/"models/HelicopterModel/GO_OH-58D.bvh"
        },
        BVHInfo{
            .key = "GO_Character",
            .path = getResourcePath()/"models/Character/GO_Character.bvh"
        },
        BVHInfo{
            .key = "GO_Goblin",
            .path = getResourcePath()/"models/Goblin/GO_Goblin.bvh"
        },
        BVHInfo{
            .key = "GO_URP_Tree_0",
            .path = getResourcePath()/"models/Trees/GO_URP_Tree_0.bvh"
        },
        BVHInfo{
            .key = "GO_URP_Tree_1",
            .path = getResourcePath()/"models/Trees/GO_URP_Tree_1.bvh"
        },
        BVHInfo{
            .key = "GO_URP_Tree_2",
            .path = getResourcePath()/"models/Trees/GO_URP_Tree_2.bvh"
        },
        BVHInfo{
            .key = "Terrain_0_0",
            .path = getResourcePath()/"terrains/BVHs/Terrain_0_0.bvh"
        },
        BVHInfo{
            .key = "Terrain_0_1",
            .path = getResourcePath()/"terrains/BVHs/Terrain_0_1.bvh"
        },
        BVHInfo{
            .key = "Terrain_0_2",
            .path = getResourcePath()/"terrains/BVHs/Terrain_0_2.bvh"
        },
        BVHInfo{
            .key = "Terrain_1_0",
            .path = getResourcePath()/"terrains/BVHs/Terrain_1_0.bvh"
        },
        BVHInfo{
            .key = "Terrain_1_1",
            .path = getResourcePath()/"terrains/BVHs/Terrain_1_1.bvh"
        },
        BVHInfo{
            .key = "Terrain_1_2",
            .path = getResourcePath()/"terrains/BVHs/Terrain_1_2.bvh"
        },
        BVHInfo{
            .key = "Terrain_2_0",
            .path = getResourcePath()/"terrains/BVHs/Terrain_2_0.bvh"
        },
        BVHInfo{
            .key = "Terrain_2_1",
            .path = getResourcePath()/"terrains/BVHs/Terrain_2_1.bvh"
        },
        BVHInfo{
            .key = "Terrain_2_2",
            .path = getResourcePath()/"terrains/BVHs/Terrain_2_2.bvh"
        }
    };

    return sAssetBVHInfos[etoi(asset)];
}