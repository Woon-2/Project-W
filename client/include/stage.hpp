#ifndef __Stage_HPP
#define __Stage_HPP

#include "ecs.hpp"
#include "systems.hpp"
#include "player.hpp"
#include "renderer.hpp"

#include "d3d12engine/d3d12Engine.hpp"

#include <vector>

class Stage {
public:
    Stage(gfx::d3d12engine::Core& core, Systems& systems, Renderer& renderer) NOEXCEPT
        : player_(), scene_(), pSystems_(&systems), pRenderer_(&renderer) {
        init();
    }

    void update(double deltaTime);
    void render(gfx::d3d12engine::Core& core);

private:
    void init();
    void processNetwork(double deltaTime);
    void processInput(double deltaTime);
    void simulate(double deltaTime);

    void initEntities();
    void initLights();
    void setupCamera();

    Player player_;
    gfx::d3d12engine::Scene scene_;
    Systems* pSystems_;
    Renderer* pRenderer_;
};

#endif  // __Stage_HPP