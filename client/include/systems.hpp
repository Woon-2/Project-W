#ifndef __Systems_HPP
#define __Systems_HPP

#include "inputSystem.hpp"
#include "physicsSystem.hpp"
#include "assetSystem.hpp"
#include "ccoord.hpp"
#include "cmodel.hpp"

struct Systems {
    AssetSystem assetSystem;
    PhysicsSystem physicsSystem;
    InputSystem inputSystem;
    CoordRoot coordRoot;

    Systems(gfx::d3d12::Core& core, ic::Keyboard& keyboard, std::size_t fenceIdx = 0)
        : assetSystem(core, fenceIdx), physicsSystem(), inputSystem(keyboard), coordRoot() {}
};

#endif  // __Systems_HPP