#ifndef __AssetMap_HPP
#define __AssetMap_HPP

#include <array>
#include <string>
#include <filesystem>

enum class AssetType {
    Texture,
    Font,
    Sound,
    Music,
    Model,
    Animation,
    Script,
    Level,
    AssetTypeCount
};

struct AssetDesc {
    using Key = std::string;
    AssetType type;
    Key key;
    std::filesystem::path path;
};

namespace detail {
extern const std::array<AssetDesc, 2> gAssetDescs;
}   // namespace detail

namespace assetIDs {
    using ID = std::size_t;

    inline constexpr ID model_gun = 0u;
    inline constexpr ID tex_gun = 1u;
}

#endif  // __AssetMap_HPP