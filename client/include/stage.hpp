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

class Quad2D {
public:
    static void setRect( ecs::Entity& entity, float x, float y, float w, float h ) {
        entity.as<gfx::d3d12engine::Model>().get().root()->coord().setLocalXform(
            mu::Mat4x4( mu::scale( mu::Vec3( w, h, 1 ) ) ) * mu::translate( mu::Vec3( x, y, 0 ) )
		);
    }
};

class LightEntity : public ecs::Entity {
public:
    void init(const gfx::d3d12::WorldLight& lightDesc) {
        createComponent<gfx::d3d12engine::Light>(lightDesc);
    }
};

class PlayerHpUIEntity : public ecs::Entity {
public:
    void init(gfx::d3d12::Texture* pTex) {
        createComponent<gameEngine::Coord>();
        createComponent<gfx::d3d12engine::Model>( gfx::d3d12::QuadModel::instance(), as<gameEngine::Coord>() );
		Quad2D::setRect( *this, 0, 0, 1024, 64 );
        // config depth sequence
        as<gfx::d3d12engine::Model>().get().root()->coord() << mu::translate( mu::Vec3(0,0,0.01) );
		// as< gfx::d3d12engine::Model>().get().root()->coord() << mu::translate( mu::Vec3( 200, 200, 0 ) );

        as<gfx::d3d12engine::Model>().get().markRenderPass( gfx::d3d12::rp::PlayerUI::id );
        as<gfx::d3d12engine::Model>().get().root()->meshes().front().submeshes().front().material().addTexRes(
            gfx::d3d12::Material::MapType::Albedo, *pTex
        );
    }
    void update(double deltaTime) {
		currentHp_ -= static_cast<float>(deltaTime * 5.0);
        if (currentHp_ < 0.0f) {
            currentHp_ = maxHp_;
		}
		Quad2D::setRect( *this, 0, 0, 1024 * (currentHp_ / maxHp_), 64 );
	}
    void onKeyInput( char key ) {
		// Quad2D::setRect( *this, 0, 0, 400 * (as<UImage>().current_ / as<UImage>().max_), 400 );
    }

private:
	float currentHp_ = 100.0f;
	float maxHp_ = 100.0f;
};

class PlayerHpFrameUIEntity : public ecs::Entity {
public:
    void init( gfx::d3d12::Texture* pTex ) {
        createComponent<gameEngine::Coord>();
        createComponent<gfx::d3d12engine::Model>( gfx::d3d12::QuadModel::instance(), as<gameEngine::Coord>() );
        Quad2D::setRect( *this, 0, 0, 1024, 64 );
        // config depth sequence
        as<gfx::d3d12engine::Model>().get().root()->coord() << mu::translate( mu::Vec3( 0, 0, 0.02 ) );

        as<gfx::d3d12engine::Model>().get().markRenderPass( gfx::d3d12::rp::PlayerUI::id );
        as<gfx::d3d12engine::Model>().get().root()->meshes().front().submeshes().front().material().addTexRes(
            gfx::d3d12::Material::MapType::Albedo, *pTex
        );
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
    std::list<ecs::Entity> entities_;

    gfx::d3d12engine::Core* pCore_;
    ControllerAdapters* pControllerAdapters_;

    ecs::Entity* pPlayer_;
    Session* pSession_;
    Systems* pSystems_;
    Renderer* pRenderer_;

    // ui
	PlayerHpUIEntity playerHpUI_;
	PlayerHpFrameUIEntity playerHpFrameUI_;
};

#endif  // __Stage_HPP