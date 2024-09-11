#include "assetMap.hpp"
#include "resourcePath.hpp"

namespace detail {
const std::array<AssetDesc, 2> gAssetDescs = {
    AssetDesc{ AssetType::Model, "Gun", resourcePath/"models"/"Gun _obj"/"Gun.obj" },
    AssetDesc{ AssetType::Texture, "GunTex", resourcePath/"models"/"Gun _obj"/"Gun.png" }
};
}   // namespace detail