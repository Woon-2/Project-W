#ifndef __D3D12Engine_HPP
#define __D3D12Engine_HPP

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

    using TextureKey = std::string;
    using RefModelKey = d3d12::RefModelStorage::ID;

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

    void loadStaticTexture(const TextureKey& key, d3d12::TextureResource::Type type);
    void loadStaticTexture(const TextureKey& key, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc);
    void loadRefModel(const RefModelKey& key);
    void layoutRefModelVBs( const d3d12::RefModelStorage::ID& key, std::size_t vbLayoutIdx,
        const d3d12::InputLayout& inputLayout
    );
    void layoutRefModelVBs(const d3d12::RefModelStorage::ID& key, std::size_t vbLayoutIdx,
        const std::vector<std::vector<Vertex::Properties>>& vbProps
    );

    void setFullScreen() {
        window_.setFullScreen(&device_);
    }

    void setWindowed() {
        window_.setWindowed(&device_);
    }

    void initChunkMesh(d3d12::D3D12GfxCmdList& cmdList) {
        d3d12::LevelChunkModel::initChunkMesh(device_, cmdList);
    }

    void registTexturePath(const TextureKey& key, const std::filesystem::path& path) {
        texturePaths_[key] = path;
    }

    void removeTexturePath(const TextureKey& key) {
        texturePaths_.erase(key);
    }

    void registRefModelPath(const RefModelKey& key, const std::filesystem::path& path) {
        refModelPaths_[key] = path;
    }

    void removeRefModelPath(const RefModelKey& key) {
        refModelPaths_.erase(key);
    }

    d3d12::DescriptorRanges& descRanges() NOEXCEPT { return descRanges_; }
    const d3d12::DescriptorRanges& descRanges() const NOEXCEPT { return descRanges_; }

    const d3d12::SamplerStorage& samStorage() const NOEXCEPT { return samStorage_; }
    const d3d12::StaticTextureStorage& staticTexStorage() const NOEXCEPT { return staticTexStorage_; }
    const d3d12::RefModelStorage& refModelStorage() const NOEXCEPT { return refModelStorage_; }

private:
    d3d12::RefModelStorage& refModelStorage() NOEXCEPT { return refModelStorage_; }
    d3d12::StaticTextureStorage& staticTexStorage() NOEXCEPT { return staticTexStorage_; }

    d3d12::StaticTextureStorage staticTexStorage_;
    d3d12::RefModelStorage refModelStorage_;
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

    std::map< TextureKey, std::filesystem::path > texturePaths_;
    std::map< RefModelKey, std::filesystem::path > refModelPaths_;
};

class Coord : public ecs::Component {
public:
    ENABLE_COMPONENT(Coord);

    Coord(const ecs::Entity& entity) NOEXCEPT
        : ecs::Component(entity) {}

    coord::System& get() NOEXCEPT { return coordSys_; }
    const coord::System& get() const NOEXCEPT { return coordSys_; }

private:
    coord::System coordSys_;
};

class CoordRoot : public ecs::System<Coord> {
public:
    void addEntity(ecs::Entity& entity);
    void update() {
        rootCoordSys_.traverse();
    }

    coord::System& get() NOEXCEPT { return rootCoordSys_; }
    const coord::System& get() const NOEXCEPT { return rootCoordSys_; }

private:
    coord::System rootCoordSys_;
};

class Model : public ecs::Component {
public:
    ENABLE_COMPONENT(Model);

    Model( const ecs::Entity& entity,
        const d3d12::RefModelStorage::ID& key, const Core& core,
        Coord& coordComp
    );

    Model( const ecs::Entity& entity,
        const d3d12::RefModel& refModel, Coord& coordComp
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
    void clearStash();

private:
    std::vector<ecs::Entity::ID> reservedEntities_;
};

class ObjectDisposition {
public:
    ObjectDisposition() = default;
    ObjectDisposition(std::ifstream& is);

    mu::Mat4x4 xform_;
    std::string name_;
    std::string prefabName_;
    std::vector<ObjectDisposition> children_;
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
    LevelRegion(const Core& core);

    void activateChunk(std::size_t xIdx, std::size_t zIdx, Scene& scene);
    std::vector<ecs::Entity> instantiateAllObjects(const Core& core, coord::System& coordRoot);

    ObjectDisposition& dispositionRoot() NOEXCEPT { return dispositionRoot_; }
    const ObjectDisposition& dispositionRoot() const NOEXCEPT { return dispositionRoot_; }

private:
    void instantiateObjectHierarchy( std::optional<std::size_t> parentIdx,
        const ObjectDisposition& disposition, const Core& core,
        coord::System& coordRoot, std::vector<ecs::Entity>& out
    );

    std::unique_ptr< std::ifstream > pTerrainStream_;
    std::unique_ptr< std::ifstream > pObjectStream_;
    // the order of the following members is important
    // ObjectDisposition's constructor reads from the stream
    // which has been already read some data from LevelRegionModel's constructor
    d3d12::LevelRegionModel model_;
    ObjectDisposition dispositionRoot_;
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

namespace rp {

class PBRIllumination : public IRenderPass, public d3d12::rp::PBRIllumination {
public:
    using d3d12::rp::PBRIllumination::PBRIllumination;

    void init(Scene& scene) override;
    void update(Scene& scene) override;
};

class ShadowMap : public IRenderPass, public d3d12::rp::ShadowMap {
public:
    using d3d12::rp::ShadowMap::ShadowMap;

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