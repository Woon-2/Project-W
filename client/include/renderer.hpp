#ifndef __renderer_HPP
#define __renderer_HPP

#include "d3d12engine/d3d12Engine.hpp"

class CubeRefModel : public gfx::d3d12::RefModel {
public:
    CubeRefModel() = default;
    CubeRefModel( gfx::d3d12engine::Core& core, gfx::d3d12::D3D12GfxCmdList& cmdList,
        float width, float height, float depth
    );
};

class CubeRP : public gfx::d3d12engine::IRenderPass, public gfx::d3d12::rp::Cube {
public:
    using gfx::d3d12::rp::Cube::Cube;

    void init(gfx::d3d12engine::Scene& scene) override;
    void update(gfx::d3d12engine::Scene& scene) override;
};

class CubeEntity : public ecs::Entity {
public:
    static void loadModel(gfx::d3d12engine::Core& core, gfx::d3d12::D3D12GfxCmdList& cmdList);
    void init();

private:
    static CubeRefModel sCubeRefModel;
};

class Renderer : public gfx::d3d12engine::IRenderer {
public:
    Renderer(gfx::d3d12engine::Core& core);

    void init(gfx::d3d12engine::Scene& scene) override;
    void render(gfx::d3d12engine::Core& core, gfx::d3d12engine::Scene& scene) override;
    void layoutVBs( gfx::d3d12engine::Core& core,
        const gfx::d3d12::RefModelStorage::ID& key,
        std::size_t layoutIdx
    );
    
private:
    gfx::d3d12::ShaderPBRIllumination shader_;
    gfx::d3d12engine::rp::PBRIllumination renderPass_;
    gfx::d3d12::ShaderCube shaderCube_;
    CubeRP renderPassCube_;
};

#endif  // __renderer_HPP