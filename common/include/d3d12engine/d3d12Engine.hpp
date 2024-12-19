#ifndef __D3D12Engine_HPP
#define __D3D12Engine_HPP

#include "d3d12engine/descriptorRangeSpec.hpp"

#include "d3d12util/d3d12Low.hpp"
#include "d3d12util/d3d12RenderPass.hpp"
#include "d3d12util/d3d12ResourceXX.hpp"
#include "d3d12util/d3d12ShaderXX.hpp"

#include "ecs.hpp"

#include <vector>

namespace gfx {

namespace d3d12engine {

inline constexpr auto initialRtvHeapSize = 3u;
inline constexpr auto initialDsvHeapSize = 1u;
inline constexpr auto initialCbvSrvUavHeapSize = 1000u;

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void init(class Scene&) = 0;
    virtual void render(class Core&, Scene&) = 0;
};

class Core {
public:
    using MyWindow = d3d12::Window<d3d12::BasicD3D12WTraits<char>>;
    friend class Model;

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

    void loadStaticTexture(const std::filesystem::path& path, d3d12::TextureResource::Type type);
    void loadRefModel(const std::filesystem::path& path, const d3d12::RefModelStorage::ID& key);\
    void layoutRefModelVBs( const d3d12::RefModelStorage::ID& key, std::size_t vbLayoutIdx,
        const d3d12::InputLayout& inputLayout
    );
    void layoutRefModelVBs(const d3d12::RefModelStorage::ID& key, std::size_t vbLayoutIdx,
        const std::vector<std::vector<Vertex::Properties>>& vbProps
    );
    void loadTerrain( const d3d12::Bitmap& heightMap, const std::filesystem::path& albedoMapPath,
        const d3d12::RefModelStorage::ID& key, mu::Vec3 scale,
        std::size_t xDivisions = 1u, std::size_t zDivisions = 1u
    );

    void setFullScreen() {
        window_.setFullScreen(&device_);
    }

    void setWindowed() {
        window_.setWindowed(&device_);
    }

private:
    d3d12::StaticTextureStorage staticTexStorage_;
    d3d12::RefModelStorage refModelStorage_;
    dx::DXGIFactory factory_;
    d3d12::D3D12Device device_;
    d3d12::D3D12CmdQueue cmdQueue_;
    d3d12::D3D12GfxCmdList cmdList_;
    d3d12::DescriptorHeapCPU rtvHeap_;
    d3d12::DescriptorHeapCPU dsvHeap_;
    d3d12::DescriptorHeapGPU cbvSrvUavHeap_;
    d3d12::DescriptorRanges descRanges_;
    MyWindow window_;
    d3d12::Fence fence_;
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
    Light(const ecs::Entity& entity, const d3d12::sr::Light& light)
        : ecs::Component(entity), light_(light) {}

    d3d12::sr::Light& get() NOEXCEPT { return light_; }
    const d3d12::sr::Light& get() const NOEXCEPT { return light_; }

private:
    d3d12::sr::Light light_;
};

class Scene;

class IRenderPass {
protected:
    ecs::SysCompCont<Model*>& models(Scene& scene);
    ecs::SysCompCont<Camera*>& cameras(Scene& scene);
    ecs::SysCompCont<Light*>& lights(Scene& scene);
    std::vector<ecs::Entity::ID>& reservedEntities(Scene& scene);

public:
    virtual void init(Scene& scene) = 0;
    virtual void update(Scene& scene) = 0;
    virtual ~IRenderPass() = default;
};

class Scene : public ecs::System<Model, Camera, Light> {
public:
    friend class IRenderPass;

    using MyBase = ecs::System<Model, Camera, Light>;

    void addEntity(ecs::Entity& entity);
    void clearStash();

private:
    std::vector<ecs::Entity::ID> reservedEntities_;
};

inline ecs::SysCompCont<Model*>& IRenderPass::models(Scene& scene) {
    return scene.components<Model>();
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

class Terrain;

class TerrainSubset : public ecs::Entity {
public:
    TerrainSubset( const d3d12::RefModelStorage::ID& key,
        Terrain* pTerrain, Core& core
    );

    TerrainSubset(const TerrainSubset&) = delete;
    TerrainSubset(TerrainSubset&& other) noexcept;
    TerrainSubset& operator=(const TerrainSubset&) = delete;
    TerrainSubset& operator=(TerrainSubset&& other) noexcept;
    ~TerrainSubset() = default;

    d3d12::RefModelStorage::ID refModelKey() const NOEXCEPT { return key_; }

private:
    d3d12::RefModelStorage::ID key_;    // store key to release later
    const Terrain* pTerrain_;
};

class Terrain : public ecs::Entity {
public:
    Terrain() = default;

    Terrain(const Terrain&) = delete;
    Terrain(Terrain&& other) noexcept;
    Terrain& operator=(const Terrain&) = delete;
    Terrain& operator=(Terrain&& other) noexcept;
    ~Terrain() = default;

    void init( const d3d12::RefModelStorage::ID& identifier,
        const std::filesystem::path& heightMapPath,
        const std::filesystem::path& albedoMapPath, mu::Vec3 scale,
        Core& core, mu::Vec3 offset = mu::Vec3(),
        std::size_t xDivisions = 1u, std::size_t zDivisions = 1u
    );

    auto& subsets() noexcept { return subsets_; }
    const auto& subsets() const noexcept { return subsets_; }

private:
    d3d12::Bitmap heightMap_;
    std::vector< std::vector<TerrainSubset> > subsets_;
    mu::Vec3 scale_;
};

namespace rp {

class PBRIllumination : public IRenderPass, public d3d12::rp::PBRIllumination {
public:
    using d3d12::rp::PBRIllumination::PBRIllumination;

    void init(Scene& scene) override;
    void update(Scene& scene) override;
};

class PBRIlluminationMacro : public IRenderPass, public d3d12::rp::PBRIlluminationMacro {
public:
    using d3d12::rp::PBRIlluminationMacro::PBRIlluminationMacro;

    void init(Scene& scene) override;
    void update(Scene& scene) override;
};

}   // namespace gfx::d3d12engine::rp

}   // namespace gfx::d3d12engine

}   // namespace gfx

#endif  // __D3D12Engine_HPP