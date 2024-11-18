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
    virtual void render(class Core&) = 0;
};

class Core {
public:
    using MyWindow = d3d12::Window<d3d12::BasicD3D12WTraits<char>>;

    Core();

    d3d12::D3D12GfxCmdList fetchCmdList() {
        return cmdList_;
    }

    static void setHInst(HINSTANCE hInstance) NOEXCEPT {
        MyWindow::setHInst(hInstance);
    }

    void render(IRenderer& renderer);
    void render();

    MyWindow& window() NOEXCEPT { return window_; }
    const MyWindow& window() const NOEXCEPT { return window_; }

    d3d12::D3D12Device& device() NOEXCEPT { return device_; }
    const d3d12::D3D12Device& device() const NOEXCEPT { return device_; }

    d3d12::UnifiedRoot& root() NOEXCEPT { return root_; }
    const d3d12::UnifiedRoot& root() const NOEXCEPT { return root_; }

    void setFullScreen() {
        window_.setFullScreen(&device_);
    }

    void setWindowed() {
        window_.setWindowed(&device_);
    }

private:
    dx::DXGIFactory factory_;
    d3d12::D3D12Device device_;
    d3d12::D3D12CmdQueue cmdQueue_;
    d3d12::D3D12GfxCmdList cmdList_;
    d3d12::DescriptorHeapCPU rtvHeap_;
    d3d12::DescriptorHeapCPU dsvHeap_;
    d3d12::DescriptorHeapGPU cbvSrvUavHeap_;
    d3d12::DescriptorRanges descRanges_;
    d3d12::UnifiedRoot root_;
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

    Model(const ecs::Entity& entity) NOEXCEPT
        : ecs::Component(entity) {}

    d3d12::Model& get() NOEXCEPT { return model_; }
    const d3d12::Model& get() const NOEXCEPT { return model_; }

private:
    d3d12::Model model_;
};

class Camera : public ecs::Component {
public:
    ENABLE_COMPONENT(Camera);

    d3d12::Camera& get() NOEXCEPT { return camera_; }
    const d3d12::Camera& get() const NOEXCEPT { return camera_; }

private:
    d3d12::Camera camera_;
};

class Light : public ecs::Component {
public:
    ENABLE_COMPONENT(Light);

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

namespace rp {

class PBRIllumination : public IRenderPass, public d3d12::rp::PBRIllumination {
public:
    using d3d12::rp::PBRIllumination::PBRIllumination;

    void init(Scene& scene) override;
    void update(Scene& scene) override;
};

}   // namespace gfx::d3d12engine::rp

}   // namespace gfx::d3d12engine

}   // namespace gfx

#endif  // __D3D12Engine_HPP