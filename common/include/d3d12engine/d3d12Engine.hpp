#ifndef __D3D12Engine_HPP
#define __D3D12Engine_HPP

#include "game/level.hpp"
#include "game/physicsSystem.hpp"
#include "game/animSystem.hpp"

#include "d3d12engine/descriptorRangeSpec.hpp"

#include "d3d12util/d3d12Low.hpp"
#include "d3d12util/d3d12RenderPass.hpp"
#include "d3d12util/d3d12ResourceXX.hpp"
#include "d3d12util/d3d12ShaderXX.hpp"

#include "ecs.hpp"

#include <vector>
#include <fstream>
#include <memory>
#include <optional>
#include <filesystem>

namespace gfx {

namespace d3d12engine {

inline constexpr auto initialRtvHeapSize = 3u;
inline constexpr auto initialDsvHeapSize = 11u;
inline constexpr auto initialSamHeapSize = 20u;
inline constexpr auto initialCbvSrvUavHeapSize = 1000u;

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void init(class Scene&) = 0;
    virtual void render(class Core&, Scene&, d3d12::RenderTargets&) = 0;
};

class Core {
public:
    using MyWindow = d3d12::Window<d3d12::BasicD3D12WTraits<char>>;
    friend class Model;
    friend class LevelRegion;

    Core();

    d3d12::D3D12GfxCmdList fetchCmdList() {
        return cmdList_;
    }

    static void setHInst(HINSTANCE hInstance) NOEXCEPT {
        MyWindow::setHInst(hInstance);
    }

    void render(IRenderer& renderer, Scene& scene);
    void render();

    MyWindow& window() NOEXCEPT { return window_; }
    const MyWindow& window() const NOEXCEPT { return window_; }

    d3d12::D3D12Device& device() NOEXCEPT { return device_; }
    const d3d12::D3D12Device& device() const NOEXCEPT { return device_; }

    d3d12::D3D12CmdQueue& cmdQueue() NOEXCEPT { return cmdQueue_; }
    const d3d12::D3D12CmdQueue& cmdQueue() const NOEXCEPT { return cmdQueue_; }

    d3d12::detail::UnifiedRootImpl& root() NOEXCEPT { return d3d12::UnifiedRoot::get(); }
    const d3d12::detail::UnifiedRootImpl& root() const NOEXCEPT { return d3d12::UnifiedRoot::get(); }

    void prepareGPUResLoad() {
        cmdList_.reset();
    }

    void finishGPUResLoad() {
        cmdList_.close();
        cmdQueue_.execute(cmdList_);

        fence_.signal(cmdQueue_);
        fence_.wait();
    }

    void setFullScreen() {
        window_.setFullScreen(&device_);
    }

    void setWindowed() {
        window_.setWindowed(&device_);
    }

    void initChunkMesh(d3d12::D3D12GfxCmdList& cmdList) {
        d3d12::LevelChunkModel::initChunkMesh(device_, cmdList);
    }

    d3d12::DescriptorRanges& descRanges() NOEXCEPT { return descRanges_; }
    const d3d12::DescriptorRanges& descRanges() const NOEXCEPT { return descRanges_; }

    const d3d12::SamplerStorage& samStorage() const NOEXCEPT { return samStorage_; }

private:
    d3d12::SamplerStorage samStorage_;
    dx::DXGIFactory factory_;
    d3d12::D3D12Device device_;
    d3d12::D3D12CmdQueue cmdQueue_;
    d3d12::D3D12GfxCmdList cmdList_;
    d3d12::DescriptorHeapCPU rtvHeap_;
    d3d12::DescriptorHeapCPU dsvHeap_;
    d3d12::DescriptorHeapGPU samHeap_;
    d3d12::DescriptorHeapGPU cbvSrvUavHeap_;
    d3d12::DescriptorRanges descRanges_;
    MyWindow window_;
    d3d12::Fence fence_;
};

class Model : public ecs::Component {
public:
    ENABLE_COMPONENT(Model);

    Model( const ecs::Entity& entity,
        const d3d12::ResourceStorage::Slot& modelSlot,
        const d3d12::ResourceStorage::ResID& modelKey,
        gameEngine::Coord& coordComp
    );

    Model( const ecs::Entity& entity,
        const d3d12::RefModel& refModel, gameEngine::Coord& coordComp
    );

    d3d12::Model& get() NOEXCEPT { return model_; }
    const d3d12::Model& get() const NOEXCEPT { return model_; }

private:
    d3d12::Model model_;
};

class LevelChunkModel : public ecs::Component {
public:
    ENABLE_COMPONENT(LevelChunkModel);

    LevelChunkModel(const ecs::Entity& entity, d3d12::LevelChunkModel& model) NOEXCEPT
        : ecs::Component(entity), pModel_(&model) {}

    d3d12::LevelChunkModel& get() NOEXCEPT { return *pModel_; }
    const d3d12::LevelChunkModel& get() const NOEXCEPT { return *pModel_; }

private:
    d3d12::LevelChunkModel* pModel_;
};

class Camera : public ecs::Component {
public:
    ENABLE_COMPONENT(Camera);

    Camera(const ecs::Entity& entity);

    Camera( const ecs::Entity& entity,
        const d3d12::Camera::Config& config
    );

    d3d12::Camera& get() NOEXCEPT { return camera_; }
    const d3d12::Camera& get() const NOEXCEPT { return camera_; }

    void update(float deltaTime);
    void attach(const Model& model) NOEXCEPT;
    void attach(const coord::System& movement, const coord::System& rotation) NOEXCEPT;
    void detach() NOEXCEPT;
    void setTimeLag(float timeLag) NOEXCEPT { timeLag_ = timeLag; }
    void MU_CALLCONV setOffset(mu::Vec3 offset) NOEXCEPT { offset_ = offset; }

private:
    d3d12::Camera camera_;
    const coord::System* pAttachedMovement_;
    const coord::System* pAttachedRotation_;
    mu::Vec3 offset_;
    float timeLag_;
};

class Light : public ecs::Component {
public:
    ENABLE_COMPONENT(Light);
    Light(const ecs::Entity& entity, const d3d12::WorldLight& light)
        : ecs::Component(entity), light_(light) {}

    d3d12::WorldLight& get() NOEXCEPT { return light_; }
    const d3d12::WorldLight& get() const NOEXCEPT { return light_; }

private:
    d3d12::WorldLight light_;
};

class Scene;

class IRenderPass {
protected:
    ecs::SysCompCont<Model*>& models(Scene& scene);
    ecs::SysCompCont<LevelChunkModel*>& levelChunkModels(Scene& scene);
    ecs::SysCompCont<Camera*>& cameras(Scene& scene);
    ecs::SysCompCont<Light*>& lights(Scene& scene);
    std::vector<ecs::Entity::ID>& reservedEntities(Scene& scene);
    std::vector<ecs::Entity::ID>& expiredEntities(Scene& scene);

public:
    virtual void init(Scene& scene) = 0;
    virtual void update(Scene& scene) = 0;
    virtual ~IRenderPass() = default;
};

class Scene : public ecs::System<Model, Camera, Light, LevelChunkModel> {
public:
    friend class IRenderPass;

    using MyBase = ecs::System<Model, Camera, Light, LevelChunkModel>;

    void addEntity(ecs::Entity& entity);
    void eraseEntity(const ecs::Entity& entity);
    void clearStash();

private:
    std::vector<ecs::Entity::ID> reservedEntities_;
    std::vector<ecs::Entity::ID> expiredEntities_;
};

class LevelChunk : public ecs::Entity {
public:
    void embed(d3d12::LevelChunkModel* pModel) {
        createComponent<LevelChunkModel>(*pModel);
    }

    // model, coord
};

class LevelRegion : public ecs::Entity {
public:
    LevelRegion() = default;
    LevelRegion( const d3d12::ResourceStorage::Slot& heightmapSlot,
        const std::filesystem::path& levelPath,
        const std::filesystem::path& levelTerrainPath
    );

    void activateChunk(std::size_t xIdx, std::size_t zIdx, Scene& scene);
    void activateChunk( std::size_t xIdx, std::size_t zIdx,
        const d3d12::ResourceStorage::Slot& bvhPathSlot,
        const d3d12::ResourceStorage::ResID& bvhPathKey, Scene& scene,
        CollisionSystem& collisionSystem
    );
    std::vector<ecs::Entity> instantiateAllObjects(
        const d3d12::ResourceStorage::Slot& refModelSlot,
        coord::System& coordRoot
    );

    gameEngine::ObjectDisposition& dispositionRoot() NOEXCEPT { return dispositionRoot_; }
    const gameEngine::ObjectDisposition& dispositionRoot() const NOEXCEPT { return dispositionRoot_; }

private:
    void instantiateObjectHierarchy( std::optional<std::size_t> parentIdx,
        const gameEngine::ObjectDisposition& disposition,
        const d3d12::ResourceStorage::Slot& refModelSlot,
        coord::System& coordRoot, std::vector<ecs::Entity>& out
    );

    std::unique_ptr< std::ifstream > pTerrainStream_;
    std::unique_ptr< std::ifstream > pObjectStream_;
    // the order of the following members is important
    // ObjectDisposition's constructor reads from the stream
    // which has been already read some data from LevelRegionModel's constructor
    d3d12::LevelRegionModel model_;
    gameEngine::ObjectDisposition dispositionRoot_;
    std::vector<LevelChunk> chunks_;
};

inline ecs::SysCompCont<Model*>& IRenderPass::models(Scene& scene) {
    return scene.components<Model>();
}

inline ecs::SysCompCont<LevelChunkModel*>& IRenderPass::levelChunkModels(Scene& scene) {
    return scene.components<LevelChunkModel>();
}

inline ecs::SysCompCont<Camera*>& IRenderPass::cameras(Scene& scene) {
    return scene.components<Camera>();
}

inline ecs::SysCompCont<Light*>& IRenderPass::lights(Scene& scene) {
    return scene.components<Light>();
}

inline std::vector<ecs::Entity::ID>& IRenderPass::reservedEntities(Scene& scene) {
    return scene.reservedEntities_;
}

inline std::vector<ecs::Entity::ID>& IRenderPass::expiredEntities(Scene& scene) {
    return scene.expiredEntities_;
}

namespace rp {

class PBRIllumination : public IRenderPass, public d3d12::rp::PBRIllumination {
public:
    using d3d12::rp::PBRIllumination::PBRIllumination;

    void init(Scene& scene) override;
    void update(Scene& scene) override;
};

class PBRAnimatedIllumination : public IRenderPass, public d3d12::rp::PBRAnimatedIllumination {
public:
    using d3d12::rp::PBRAnimatedIllumination::PBRAnimatedIllumination;

    void init(Scene& scene) override;
    void update(Scene& scene) override;
};

class ShadowMap : public IRenderPass, public d3d12::rp::ShadowMap {
public:
    using d3d12::rp::ShadowMap::ShadowMap;

    void init(Scene& scene) override;
    void update(Scene& scene) override;
};

class CascadeShadowMap : public IRenderPass, public d3d12::rp::CascadeShadowMap {
public:
    using d3d12::rp::CascadeShadowMap::CascadeShadowMap;

    void init(Scene& scene) override;
    void update(Scene& scene) override;
};

class ShadowMapAnimated : public IRenderPass, public d3d12::rp::ShadowMapAnimated {
public:
    using d3d12::rp::ShadowMapAnimated::ShadowMapAnimated;

    void init(Scene& scene) override;
    void update(Scene& scene) override;
};

class ScreenQuad : public IRenderPass, public d3d12::rp::ScreenQuad {
public:
    using d3d12::rp::ScreenQuad::ScreenQuad;

    void init(Scene& scene) override;
    void update(Scene& scene) override;
};

class Tessellation : public IRenderPass, public d3d12::rp::Tessellation {
public:
    using d3d12::rp::Tessellation::Tessellation;

    void init(Scene& scene) override;
    void update(Scene& scene) override;
};

class ShadowMapTessellation : public IRenderPass, public d3d12::rp::ShadowMapTessellation {
public:
    using d3d12::rp::ShadowMapTessellation::ShadowMapTessellation;

    void init(Scene& scene) override;
    void update(Scene& scene) override;
};

}   // namespace gfx::d3d12engine::rp

}   // namespace gfx::d3d12engine

}   // namespace gfx

#endif  // __D3D12Engine_HPP