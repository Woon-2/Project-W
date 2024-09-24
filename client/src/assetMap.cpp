#include "assetMap.hpp"
#include "resourcePath.hpp"

namespace detail {
const std::array<std::vector<AssetDesc>, 1> gAssetDescs = { { {
    AssetDesc{ AssetType::Model, "DragonModel", resourcePath/"models"/"dragon"/"Dragon 2.5_fbx.fbx" },
    AssetDesc{ AssetType::Texture, "DragonTex", resourcePath/"models"/"dragon"/"textures"/"Dragon_ground_color.jpg" },
    AssetDesc{ AssetType::Texture, "DragonNormalMap", resourcePath/"models"/"dragon"/"textures"/"Dragon_Nor.jpg" },
    AssetDesc{ AssetType::Texture, "DragonBumpMap", resourcePath/"models"/"dragon"/"textures"/"Dragon_Bump_Col2.jpg" },
    AssetDesc{ AssetType::Texture, "FloorTex", resourcePath/"models"/"dragon"/"textures"/"Floor_C.jpg"},
    AssetDesc{ AssetType::Texture, "FloorNormalMap", resourcePath/"models"/"dragon"/"textures"/"Floor_N.jpg"},
    AssetDesc{ AssetType::Texture, "FloorSpecularMap", resourcePath/"models"/"dragon"/"textures"/"Floor_S.jpg"},
    AssetDesc{ AssetType::MaterialTree, "DragonMaterialTree", resourcePath/"models"/"dragon"/"Dragon 2.5_fbx.fbx" }
} } };
}   // namespace detail