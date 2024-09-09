#ifndef __D3d12Material_HPP
#define __D3d12Material_HPP

#include "d3d12texture.hpp"

#include "renderProtocol.hpp"

#include <map>
#include <ranges>
#include <algorithm>
#include <cstdint>
#include <cassert>
#include <any>

namespace gfx {

namespace d3d12 {

class Material {
public:
    // A struct for holding a texture and its index in the srv heap (for bindless resources)
    struct TextureIndexed {
        Texture tex;
        std::uint32_t idx;
    };

    enum class Properties {
        Diffuse,
        Normal,
        Specular,
        Emissive,
        Roughness,
        Metallic,
        AmbientOcclusion,
        Height,
        Opacity,
        Max
    };

    Material() = default;
    template <std::ranges::sized_range RProp, std::ranges::sized_range RTex>
    Material(Core& core, RProp&& props, RTex&& texs, std::any&& constants = std::any{})
        : textures_(), constants_(std::move(constants)) {
        assert(std::ranges::size(props) == std::ranges::size(texs));

        auto propIt = std::ranges::begin(props);
        auto texIt = std::ranges::begin(texs);
        while (propIt != std::ranges::end(props)) {
            pushTexture(core, *propIt, *texIt);
            ++propIt;
            ++texIt;
        }
    }

    void pushTexture(Core& core, Properties prop, const Texture& tex);
    void pushTexture(Core& core, Properties prop, Texture&& tex);

    const Texture& texture(Properties prop) const {
        return textures_.at(prop).tex;
    }

    const std::any& constants() const NOEXCEPT { return constants_; }
    void writeConstants(std::any&& constants) NOEXCEPT { constants_ = std::move(constants); }

    bool canSupport(rp::Protocol protocol) const;
    std::any as(rp::Protocol protocol) const;

private:
    rp::PhongInstancing::MaterialType asPhongInstancing() const;
    rp::PhongInstancingNT::MaterialType asPhongInstancingNT() const;

    std::map<Properties, TextureIndexed> textures_;
    std::any constants_;
};

}   // namespace gfx::d3d12

}   // namespace gfx

#endif // __D3d12Material_HPP