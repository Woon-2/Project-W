#ifndef __Stage_HPP
#define __Stage_HPP

#include "stdafx.hpp"

#include "net/netInclude.hpp"
#include "net/protocol.hpp"

#include "session.hpp"
#include "ecs.hpp"
#include "systems.hpp"
#include "renderer.hpp"

#include "d3d12engine/d3d12Engine.hpp"


class LightEntity : public ecs::Entity {
public:
    void init(const gfx::d3d12::WorldLight& lightDesc) {
        createComponent<gfx::d3d12engine::Light>(lightDesc);
    }
};

class Stage {
public:
    static constexpr auto slotKeyTexture = "Texture";
    static constexpr auto slotKeyTexArray = "TextureArray";
    static constexpr auto slotKeyTexCube = "TextureCube";
    static constexpr auto slotKeyModel = "Model";
    static constexpr auto slotKeyBVHPath = "BVHPath";
    static constexpr auto slotKeySkeleton = "Skeleton";
    static constexpr auto slotKeyAnimClip = "AnimClip";

    Stage( gfx::d3d12engine::Core& core, Systems& systems,
        ControllerAdapters& controllerAdapters, Renderer& renderer, Session& session
    ) NOEXCEPT
        : staticResStorage_(), directionalLight_(), level_(), scene_(), entities_(), pCore_(&core),
        pControllerAdapters_(&controllerAdapters), pPlayer_(nullptr),
        pSession_(&session), pSystems_(&systems), pRenderer_(&renderer) {
        init();
    }

    void addEntity(ecs::Entity&& entity);
    void update(double deltaTime);
    void render();

    gfx::d3d12::ResourceStorage& resStorage() NOEXCEPT {
        return staticResStorage_;
    }

    void setPlayer(ecs::Entity* player) NOEXCEPT {
        pPlayer_ = player;
    }

    auto& entities() NOEXCEPT {
        return entities_;
    }

    auto pSystems() NOEXCEPT {
        return pSystems_;
    }

    auto pScene() NOEXCEPT {
        return &scene_;
    }

    void initScene();

private:
    void init();
    void prepareResStorage();
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

    gfx::d3d12::ResourceStorage staticResStorage_;

    LightEntity directionalLight_;
    gfx::d3d12engine::LevelRegion level_;
    gfx::d3d12engine::Scene scene_;
    std::vector<ecs::Entity> entities_;

    gfx::d3d12engine::Core* pCore_;
    ControllerAdapters* pControllerAdapters_;

    ecs::Entity* pPlayer_;
    Session* pSession_;
    Systems* pSystems_;
    Renderer* pRenderer_;
};

#endif  // __Stage_HPP