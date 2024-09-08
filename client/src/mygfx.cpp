#include "mygfx.hpp"

#include "rootPresets.hpp"
#include "d3d12InputLayoutPresets.hpp"
#include "d3d12texture.hpp"
#include "phongRenderer.hpp"

void MyGfx::init() {
    pRenderer_ = std::make_unique<gfx::PhongRendererNT>();

    gfx::d3d12::Core::init();
    addRoot( gfx::d3d12::rootName(gfx::d3d12::RootPreset::Unified),
        gfx::d3d12::makeRootPreset(*this, gfx::d3d12::RootPreset::Unified)
    );
    addRoot( gfx::d3d12::rootName(gfx::d3d12::RootPreset::Unified1),
        gfx::d3d12::makeRootPreset(*this, gfx::d3d12::RootPreset::Unified1)
    );

    gfx::d3d12::configInputLayoutAux(gfx::Vertex::Properties::Position);
    gfx::d3d12::configInputLayoutAux(gfx::Vertex::Properties::Normal);
    gfx::d3d12::configInputLayoutAux(gfx::Vertex::Properties::TexCoord);
    addInputLayout( gfx::d3d12::inputLayoutName(gfx::InputLayoutPreset::Pos3Norm3),
        gfx::makeInputLayoutPreset(gfx::InputLayoutPreset::Pos3Norm3)
    );
    addInputLayout(gfx::d3d12::inputLayoutName(gfx::InputLayoutPreset::Pos3Norm3Tex2),
        gfx::makeInputLayoutPreset(gfx::InputLayoutPreset::Pos3Norm3Tex2)
    );

    auto pDevice = static_cast<ID3D12Device*>( gfx::d3d12::DeviceFetcher::device(*this) );

    addDescHeap( gfx::d3d12::Texture::texSrvHeapIdx,
        gfx::d3d12::DescriptorHeap(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 123u, true)
    );

    pRenderer_->init(*this);
}