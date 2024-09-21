#ifndef __D3d12Material_HPP
#define __D3d12Material_HPP

#include "d3d12texture.hpp"

#include "renderProtocol.hpp"

#include "enumUtil.hpp"

#define DXMATH_VEC_UTIL
#define DXMATH_MAT_UTIL
#define DXMATH_QUAT_UTIL
#include "mathUtil.hpp"

#include <vector>
#include <ranges>
#include <algorithm>
#include <cstdint>
#include <cassert>
#include <variant>
#include <optional>

namespace gfx {

namespace d3d12 {

class Material {
public:
    using TextureIdx = std::uint32_t;

    enum class Maps {
        Diffuse,
        Ambient,
        Specular,
        Emissive,
        Normal,
        Roughness,
        Metallic,
        AmbientOcclusion,
        Height,
        Opacity,
        Max
    };

    enum class Properties {
        BaseColor,
        DiffuseColor,
        SpecularColor,
        EmissiveColor,
        AmbientColor,
        AnistrophyFactor,
        BumpScaling,
        Shininess,
        ShininessStrength,
        RoughnessFactor,
        MetallicFactor,
        Opacity,
        MappingMode_U_Diffuse,
        MappingMode_V_Diffuse,
        MappingMode_U_Normal,
        MappingMode_V_Normal,
        MappingMode_U_Specular,
        MappingMode_V_Specular,
        MappingMode_U_Emissive,
        MappingMode_V_Emissive,
        MappingMode_U_Roughness,
        MappingMode_V_Roughness,
        MappingMode_U_Metallic,
        MappingMode_V_Metallic,
        MappingMode_U_AmbientOcclusion,
        MappingMode_V_AmbientOcclusion,
        MappingMode_U_Height,
        MappingMode_V_Height,
        MappingMode_U_Opacity,
        MappingMode_V_Opacity,
        Max
    };

    enum class MappingMode {
        Wrap,
        Mirror,
        Clamp,
        Decal
    };

    union Property {
        mu::Vec4 vec4;
        float scalar;
        MappingMode mappingMode;
    };

    Material() = default;
    template < std::ranges::sized_range RTexKeys, std::ranges::sized_range RTex,
        std::ranges::sized_range RPropKeys, std::ranges::sized_range RProp
    >
    Material( Core& core, const RTexKeys& texKeys, const RTex& texs,
        const RPropKeys& propKeys, const RProp& props
    ) : indices_(etoi(Maps::Max), TextureIdx(-1)), properties_(etoi(Properties::Max)) {
        assert(std::ranges::size(texKeys) == std::ranges::size(texs));
        assert(std::ranges::size(propKeys) == std::ranges::size(props));

        auto texKeyIt = std::ranges::begin(texKeys);
        auto texIt = std::ranges::begin(texs);
        while (texKeyIt != std::ranges::end(texKeys)) {
            pushTexture(core, *texKeyIt, *texIt);
            ++texKeyIt;
            ++texIt;
        }

        auto propKeyIt = std::ranges::begin(propKeys);
        auto propIt = std::ranges::begin(props);
        while (propKeyIt != std::ranges::end(propKeys)) {
            pushProperty(*propKeyIt, *propIt);
            ++propKeyIt;
            ++propIt;
        }
    }

    void pushTexture(Core& core, Maps prop, const Texture& tex);
    const TextureIdx idx(Maps prop) const;

    void pushProperty(Properties prop, Property val) {
        properties_[etoi(prop)] = val;
    }
    const Property property(Properties prop) const {
        if (!properties_[etoi(prop)].has_value()) {
            throw std::runtime_error("Property not found");
        }
        return properties_[etoi(prop)].value();
    }

    bool canSupport(rp::Protocol protocol) const;
    std::any as(rp::Protocol protocol) const;

private:
    bool contains(Maps map) const NOEXCEPT {
        return indices_[etoi(map)] != TextureIdx(-1);
    }

    bool contains(Properties prop) const NOEXCEPT {
        return properties_[etoi(prop)].has_value();
    }

    rp::PhongInstancing::MaterialType asPhongInstancing() const;
    rp::PhongInstancingNT::MaterialType asPhongInstancingNT() const;

    std::vector<TextureIdx> indices_;
    std::vector<std::optional<Property>> properties_;
};

}   // namespace gfx::d3d12

}   // namespace gfx

#endif // __D3d12Material_HPP