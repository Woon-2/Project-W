#include "mygfx.hpp"

#include "rootPresets.hpp"
#include "d3d12InputLayoutPresets.hpp"
#include "d3d12texture.hpp"

void MyGfx::init() {
    configRtvHeapSize(3u);
    configDsvHeapSize(3u);
    configCbvSrvUavHeapSize(124u);

    defineDescRange("offscreenRtv", 0u, 3u);
    defineDescRange("frameDsv", 0u, 3u);
    defineDescRange(gfx::rp::ShadowMapGen::DescRangeIDShadowTex, 0u, 1u);
    defineDescRange(gfx::rp::PhongInstancing::DescRangeIDTex2D, 1u, 123u);

    gfx::d3d12::Core::init();

    addRoot( gfx::d3d12::rootName(gfx::d3d12::RootPreset::Unified),
        gfx::d3d12::makeRootPreset(*this, gfx::d3d12::RootPreset::Unified)
    );
    addRoot( gfx::d3d12::rootName(gfx::d3d12::RootPreset::Unified1),
        gfx::d3d12::makeRootPreset(*this, gfx::d3d12::RootPreset::Unified1)
    );

    auto pDevice = static_cast<ID3D12Device*>( gfx::d3d12::DeviceFetcher::device(*this) );

    phongShader_ = gfx::d3d12::PhongShader( *this,
        gfx::d3d12::PhongShader::Config{ .maxInstCnt = 1000u, .maxLightCnt = 12u },
        2u
    );

    phongShaderNT_ = gfx::d3d12::PhongShaderNT( *this,
        gfx::d3d12::PhongShaderNT::Config{ .maxInstCnt = 1000u, .maxLightCnt = 12u },
        2u
    );

    illuminanceRenderer_.init(*this);
    illuminanceRenderer_.pushShader( gfx::rp::Protocol::PhongInstancing, &phongShader_.value(),
        phongShader_.value().optSolidAndGeneral()
    );
    illuminanceRenderer_.pushShader( gfx::rp::Protocol::PhongInstancingNT, &phongShaderNT_.value(),
        phongShaderNT_.value().optSolidAndGeneral()
    );
}

void MyGfx::setFrame(std::size_t frameIdx) {
    phongShader_.value().setFrame(frameIdx);
    phongShaderNT_.value().setFrame(frameIdx);
}