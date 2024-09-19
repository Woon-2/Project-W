#ifndef __D3d12Material_HPP
#define __D3d12Material_HPP

#include "d3d12texture.hpp"

#include "renderProtocol.hpp"

#include "enumUtil.hpp"

#include <vector>
#include <ranges>
#include <algorithm>
#include <cstdint>
#include <cassert>
#include <any>

namespace gfx {

namespace d3d12 {

class Material {
public:
    using TextureIdx = std::uint32_t;

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
    Material(Core& core, RProp&& props, const RTex& texs, std::any&& constants = std::any{})
        : indices_(etoi(Properties::Max), TextureIdx(-1)), constants_(std::move(constants)) {
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
    const TextureIdx idx(Properties prop) const;

    const std::any& constants() const NOEXCEPT { return constants_; }
    void writeConstants(std::any&& constants) NOEXCEPT { constants_ = std::move(constants); }

    bool canSupport(rp::Protocol protocol) const;
    std::any as(rp::Protocol protocol) const;

private:
    bool contains(Properties prop) const {
        return indices_[etoi(prop)] != TextureIdx(-1);
    }

    rp::PhongInstancing::MaterialType asPhongInstancing() const;
    rp::PhongInstancingNT::MaterialType asPhongInstancingNT() const;

    std::vector<TextureIdx> indices_;
    std::any constants_;
};

}   // namespace gfx::d3d12

}   // namespace gfx

#endif // __D3d12Material_HPP