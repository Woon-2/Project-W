#ifndef __Stage_HPP
#define __Stage_HPP

#include "net/netInclude.hpp"
#include "net/protocol.hpp"
#include "net/session.hpp"

#include "ecs.hpp"
#include "systems.hpp"
#include "renderer.hpp"

#include "d3d12engine/d3d12Engine.hpp"

#include <vector>

class LightEntity : public ecs::Entity {
public:
    void init(const gfx::d3d12::WorldLight& lightDesc) {
        createComponent<gfx::d3d12engine::Light>(lightDesc);
    }
};

class Stage {
public:
    Stage(gfx::d3d12engine::Core& core, Systems& systems, Renderer& renderer, Session& session) NOEXCEPT
        : directionalLight_(), level_(), scene_(), entities_(), pCore_(&core),
        pPlayer_(nullptr), pSession_(&session), pSystems_(&systems), pRenderer_(&renderer) {
        init();
    }

    void update(double deltaTime);
    void render();

private:
    void init();
    void loadAssets();
    void loadTextures(gfx::d3d12::D3D12GfxCmdList& cmdList);
    void loadModels(gfx::d3d12::D3D12GfxCmdList& cmdList);
    void loadBVHPaths();
    void loadLevel(gfx::d3d12::D3D12GfxCmdList& cmdList);
    void processPackets(double deltaTime);
    void updateNetwork(double deltaTime);
    void processInput(double deltaTime);
    void simulate(double deltaTime);

    void initEntities();
    void initScene();

    LightEntity directionalLight_;
    gfx::d3d12engine::LevelRegion level_;
    gfx::d3d12engine::Scene scene_;
    std::vector<ecs::Entity> entities_;

    gfx::d3d12engine::Core* pCore_;
    ecs::Entity* pPlayer_;
    Session* pSession_;
    Systems* pSystems_;
    Renderer* pRenderer_;
};

#endif  // __Stage_HPP