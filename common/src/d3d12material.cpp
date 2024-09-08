#include "d3d12material.hpp"

namespace gfx {
    
namespace d3d12 {

void Material::pushTexture(Core& core, Properties prop, const Texture& tex) {
    if (textures_.contains(prop)) {
        throw std::runtime_error("Texture already exists for this property");
    }

    auto& texSrvHeap = core.descHeap(Texture::texSrvHeapIdx);
    auto texSrvHeapStart = texSrvHeap.gpuHandle();
    auto srvStride = texSrvHeap.stride();
    auto texSrvAddr = tex.gpuHandle();
    auto idx = static_cast<std::uint32_t>( (texSrvAddr.ptr - texSrvHeapStart.ptr) / srvStride );

    textures_.try_emplace(prop, TextureIndexed{tex, idx});
}

void Material::pushTexture(Core& core, Properties prop, Texture&& tex) {
    if (textures_.contains(prop)) {
        throw std::runtime_error("Texture already exists for this property");
    }

    auto& texSrvHeap = core.descHeap(Texture::texSrvHeapIdx);
    auto texSrvHeapStart = texSrvHeap.gpuHandle();
    auto srvStride = texSrvHeap.stride();
    auto texSrvAddr = tex.gpuHandle();
    auto idx = static_cast<std::uint32_t>( (texSrvAddr.ptr - texSrvHeapStart.ptr) / srvStride );

    textures_.try_emplace(prop, TextureIndexed{std::move(tex), idx});
}

std::any Material::as(rp::Protocol protocol) const {
    switch (protocol) {
    case rp::Protocol::PhongInstancingNT:
        return asPhongInstancingNT();
    case rp::Protocol::PhongInstancing:
        return asPhongInstancing();
    default:
        throw std::runtime_error("Unsupported protocol");
    }

    return {};
}

rp::PhongInstancing::MaterialType Material::asPhongInstancing() const {
    if (constants_.type() != typeid(float)) {
        throw std::runtime_error("PhongInstancing render protocol's Material Auxiliary data must be of type float.");
    }

    auto ret = rp::PhongInstancing::MaterialType{
        .diffuseMapIdx = textures_.at(Properties::Diffuse).idx,
        .specularMapIdx = textures_.at(Properties::Specular).idx,
        .shininess = std::any_cast<float>(constants_)
    };

    return ret;
}

rp::PhongInstancingNT::MaterialType Material::asPhongInstancingNT() const
{
    if ( constants_.type() != typeid( rp::PhongInstancingNT::MaterialType ) ) {
        throw std::runtime_error("PhongInstancingNT render protocol's Material Auxiliary data must be of type PhongMaterialNT.");
    }

    return std::any_cast<rp::PhongInstancingNT::MaterialType>( constants_ );
}

}   // namespace gfx::d3d12

}   // namespace gfx