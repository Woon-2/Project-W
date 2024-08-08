#include "mygfx.hpp"

#include "rootPresets.hpp"
#include "sampleRenderer.hpp"

void MyGfx::init() {
    pRenderer_ = std::make_unique<gfx::SampleRenderer>();

    gfx::d3d12::Core::init();
    addRoot(0, gfx::d3d12::makeRootPreset(*this, gfx::d3d12::RootPreset::Null));
    mapRoot(*pRenderer_, 0);

    pRenderer_->init(*this);
}