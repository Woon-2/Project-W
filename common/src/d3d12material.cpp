#include "d3d12material.hpp"

namespace gfx {
    
namespace d3d12 {

void Material::pushTexture(Core& core, Properties prop, const Texture& tex) {
    if (contains(prop)) {
        throw std::runtime_error("Texture already exists for this property");
    }

    auto& texSrvHeap = core.descHeap(Texture::texSrvHeapIdx);
    auto texSrvHeapStart = texSrvHeap.gpuHandle();
    auto srvStride = texSrvHeap.stride();
    auto texSrvAddr = tex.gpuHandle();
    auto idx = static_cast<std::uint32_t>( (texSrvAddr.ptr - texSrvHeapStart.ptr) / srvStride );

    indices_[etoi(prop)] = idx;
}

const Material::TextureIdx Material::idx(Properties prop) const {
    if (!contains(prop)) {
        throw std::runtime_error("Texture not found for this property");
    }
    return indices_[etoi(prop)];
}

bool Material::canSupport(rp::Protocol protocol) const {
    switch(protocol) {
    case rp::Protocol::PhongInstancingNT: {
        auto ret = constants_.type() == typeid(rp::PhongInstancingNT::MaterialType);
        return ret;
    }

    case rp::Protocol::PhongInstancing: {
        auto ret = constants_.type() == typeid(float);
        ret = ret && contains(Properties::Diffuse);
        ret = ret && contains(Properties::Specular);
        return ret;
    }

    default:
        throw std::runtime_error("Undefined protocol");
    }

    return false;
}

std::any Material::as(rp::Protocol protocol) const {
    switch (protocol) {
    case rp::Protocol::PhongInstancingNT:
        return asPhongInstancingNT();
    case rp::Protocol::PhongInstancing:
        return asPhongInstancing();
    default:
        throw std::runtime_error("Undefined protocol");
    }

    return {};
}

rp::PhongInstancing::MaterialType Material::asPhongInstancing() const {
    if (constants_.type() != typeid(float)) {
        throw std::runtime_error("PhongInstancing render protocol's Material Auxiliary data must be of type float.");
    }
    if (!contains(Properties::Diffuse) || !contains(Properties::Specular)) {
        throw std::runtime_error("PhongInstancing render protocol's Material must have Diffuse and Specular textures.");
    }

    auto ret = rp::PhongInstancing::MaterialType{
        .diffuseMapIdx = indices_[etoi(Properties::Diffuse)],
        .specularMapIdx = indices_[etoi(Properties::Specular)],
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

} // namespace gfx