#include "mygfx.hpp"

#include "rootPresets.hpp"
#include "d3d12InputLayoutPresets.hpp"
#include "sampleRenderer.hpp"

void MyGfx::init() {
    pRenderer_ = std::make_unique<gfx::SampleRenderer>();

    gfx::d3d12::Core::init();
    addRoot( gfx::d3d12::rootName(gfx::d3d12::RootPreset::Null),
        gfx::d3d12::makeRootPreset(*this, gfx::d3d12::RootPreset::Null)
    );
    addRoot( gfx::d3d12::rootName(gfx::d3d12::RootPreset::Solid),
        gfx::d3d12::makeRootPreset(*this, gfx::d3d12::RootPreset::Solid)
    );

    gfx::d3d12::configInputLayoutAux(gfx::Vertex::Properties::Position);
    addInputLayout( gfx::d3d12::inputLayoutName(gfx::InputLayoutPreset::Pos3),
        gfx::makeInputLayoutPreset(gfx::InputLayoutPreset::Pos3)
    );

    pRenderer_->init(*this);
}