#include "d3d12material.hpp"

namespace gfx {
    
namespace d3d12 {

void Material::pushTexture(Core& core, Maps prop, const Texture& tex) {
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

const Material::TextureIdx Material::idx(Maps prop) const {
    if (!contains(prop)) {
        throw std::runtime_error("Texture not found for this property");
    }
    return indices_[etoi(prop)];
}

bool Material::canSupport(rp::Protocol protocol) const {
    switch(protocol) {
    case rp::Protocol::PhongInstancingNT: {
        auto ret = contains(Properties::DiffuseColor);
        ret = ret && contains(Properties::SpecularColor);
        ret = ret && contains(Properties::AmbientColor);
        ret = ret && contains(Properties::EmissiveColor);
        return ret;
    }

    case rp::Protocol::PhongInstancing: {
        auto ret = contains(Properties::Shininess);
        ret = ret && contains(Maps::Diffuse);
        ret = ret && contains(Maps::Specular);
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
    if ( !contains(Maps::Diffuse) || !contains(Maps::Specular)
        || !contains(Properties::Shininess)
    ) {
        throw std::runtime_error("PhongInstancing render protocol's Material must have "
            "Diffuse and Specular textures and shininess property."
        );
    }

    auto ret = rp::PhongInstancing::MaterialType{
        .diffuseMapIdx = indices_[etoi(Maps::Diffuse)],
        .specularMapIdx = indices_[etoi(Maps::Specular)],
        .shininess = properties_[etoi(Properties::Shininess)].value().scalar
    };

    return ret;
}

rp::PhongInstancingNT::MaterialType Material::asPhongInstancingNT() const
{
    if ( !contains(Properties::DiffuseColor) || !contains(Properties::SpecularColor)
        || !contains(Properties::AmbientColor) || !contains(Properties::EmissiveColor)
    ) {
        throw std::runtime_error("PhongInstancingNT render protocol's Material must have "
            "Diffuse, Specular, Ambient and Emissive colors."
        );
    }

    return rp::PhongInstancingNT::MaterialType{
        .ambient = properties_[etoi(Properties::AmbientColor)].value().vec4.getXmf(),
        .diffuse = properties_[etoi(Properties::DiffuseColor)].value().vec4.getXmf(),
        .specular = properties_[etoi(Properties::SpecularColor)].value().vec4.getXmf(),
        .emmisive = properties_[etoi(Properties::EmissiveColor)].value().vec4.getXmf()
    };
}

}   // namespace gfx::d3d12

} // namespace gfx