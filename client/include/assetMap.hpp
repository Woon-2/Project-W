#ifndef __AssetMap_HPP
#define __AssetMap_HPP

#include <array>
#include <vector>
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
    MaterialTree,
    AssetTypeCount
};

struct AssetDesc {
    using Key = std::string;
    AssetType type;
    Key key;
    std::filesystem::path path;
};

namespace detail {
extern const std::array<std::vector<AssetDesc>, 1> gAssetDescs;
}   // namespace detail

namespace assetIDs {
    using ID = std::size_t;

    inline constexpr ID dragon = 0u;
}

#endif  // __AssetMap_HPP