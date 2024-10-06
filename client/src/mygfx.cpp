#include "mygfx.hpp"

#include "rootPresets.hpp"
#include "d3d12InputLayoutPresets.hpp"
#include "d3d12texture.hpp"

void MyGfx::init() {
    gfx::d3d12::Core::init();

    phongShader_ = gfx::d3d12::PhongShader( *this,
        gfx::d3d12::PhongShader::Config{ .maxInstCnt = 1000u, .maxLightCnt = 12u },
        2u
    );

    phongShaderNT_ = gfx::d3d12::PhongShaderNT( *this,
        gfx::d3d12::PhongShaderNT::Config{ .maxInstCnt = 1000u, .maxLightCnt = 12u },
        2u
    );

    addRoot( gfx::d3d12::rootName(gfx::d3d12::RootPreset::Unified),
        gfx::d3d12::makeRootPreset(*this, gfx::d3d12::RootPreset::Unified)
    );
    addRoot( gfx::d3d12::rootName(gfx::d3d12::RootPreset::Unified1),
        gfx::d3d12::makeRootPreset(*this, gfx::d3d12::RootPreset::Unified1)
    );

    auto pDevice = static_cast<ID3D12Device*>( gfx::d3d12::DeviceFetcher::device(*this) );

    addDescHeap( gfx::d3d12::Texture::texSrvHeapIdx,
        gfx::d3d12::DescriptorHeap(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 123u, true)
    );

    illuminanceRenderer_.init(*this);
    illuminanceRenderer_.pushShader( gfx::rp::Protocol::PhongInstancing, &phongShader_.value() );
    illuminanceRenderer_.pushShader( gfx::rp::Protocol::PhongInstancingNT, &phongShaderNT_.value() );
}

void MyGfx::setFrame(std::size_t frameIdx) {
    phongShader_.value().setFrame(frameIdx);
    phongShaderNT_.value().setFrame(frameIdx);
}