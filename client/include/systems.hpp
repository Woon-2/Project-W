#ifndef __Systems_HPP
#define __Systems_HPP

#include "inputSystem.hpp"
#include "physicsSystem.hpp"

#include "d3d12engine/d3d12Engine.hpp"

struct Systems {
    PhysicsSystem physicsSystem;
    InputSystem inputSystem;
    gfx::d3d12engine::CoordRoot coordRoot;

    Systems(ic::Keyboard& keyboard, std::size_t fenceIdx = 0)
        : physicsSystem(), inputSystem(keyboard), coordRoot() {}
};

#endif  // __Systems_HPP