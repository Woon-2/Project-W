#include "mygfx.hpp"

#include "rootPresets.hpp"
#include "sampleRenderer.hpp"

void MyGfx::init() {
    pRenderer_ = std::make_unique<gfx::SampleRenderer>();

    gfx::d3d12::Core::init();
    addRoot( gfx::d3d12::rootName(gfx::d3d12::RootPreset::Null),
        gfx::d3d12::makeRootPreset(*this, gfx::d3d12::RootPreset::Null)
    );

    pRenderer_->init(*this);
}