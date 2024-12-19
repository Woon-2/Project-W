#ifndef __renderer_HPP
#define __renderer_HPP

#include "d3d12engine/d3d12Engine.hpp"

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
};

#endif  // __renderer_HPP