#ifndef __Stage_HPP
#define __Stage_HPP

#include "ecs.hpp"
#include "systems.hpp"

#include "player.hpp"
#include "camera.hpp"

#include "d3d12core.hpp"

#include <vector>

class Stage {
public:
    Stage(gfx::d3d12::Core& core, const Win32::WndClient& client, Systems& systems) NOEXCEPT
        : camera_( gfx::Camera::Config{
        .fov = 90.f, .aspect = client.width / static_cast<float>(client.height),
        .near = 0.1f, .far = 10000.f
    } ), player_(), pSystems_(&systems) {
        init();
    }

    void update(double deltaTime);
    void render(gfx::ICore& core, gfx::IRenderTarget& target);

private:
    void init();
    void processNetwork(double deltaTime);
    void processInput(double deltaTime);
    void simulate(double deltaTime);

    void initEntities();
    void initLights();
    void setupCamera();

    gfx::Camera camera_;
    Player player_;
    Systems* pSystems_;
    std::vector< gfx::d3d12::sr::PhongLight > lights_;
};

#endif  // __Stage_HPP