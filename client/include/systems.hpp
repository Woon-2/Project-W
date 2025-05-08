#ifndef __Systems_HPP
#define __Systems_HPP

#include "inputSystem.hpp"
#include "game/physicsSystem.hpp"
#include "game/animSystem.hpp"
#include "cNetEx.hpp"

#include "d3d12engine/d3d12Engine.hpp"

struct Systems {
    PhysicsSystem physicsSystem;
    InputSystem inputSystem;
    gameEngine::CoordRoot coordRoot;
    CollisionSystem collisionSystem;
    CNetExSystem netSystem;
    AnimSystem animSystem;

    Systems(gfx::d3d12engine::Core& core, ic::Keyboard* pKeyboard, ic::Mouse* pMouse, std::size_t fenceIdx = 0)
        : physicsSystem(), inputSystem(pKeyboard, pMouse), coordRoot(),
        collisionSystem(), netSystem(),
        animSystem(core.device(), core.root()) {}
};

#endif  // __Systems_HPP