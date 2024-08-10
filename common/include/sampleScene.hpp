#ifndef __SAMPLE_SCENE_HPP
#define __SAMPLE_SCENE_HPP

#include "d3d12core.hpp"

#include "gfxPrimitive.hpp"

#include "d3d12mesh.hpp"
#include "d3d12InputLayoutPresets.hpp"
#include "resourcePath.hpp"

#include <array>

namespace gfx {

class SampleDrawable {
public:
    static constexpr std::size_t vbIdx = 0;
    static constexpr std::size_t ibIdx = 1;

    SampleDrawable(d3d12::Core& core, d3d12::D3D12RenderContext& ctx)
        : mesh_( core, ctx, loadMesh(resourcePath/"models"/"Gun _obj"/"Gun.obj",
            core.inputLayout( d3d12::inputLayoutName(InputLayoutPreset::Pos3) )
        ), "Gun_vs", "Gun_ps" ) {}

    void completeInit(d3d12::Core& core);

    d3d12::Mesh& mesh() { return mesh_; }
    const d3d12::Mesh& mesh() const { return mesh_; }

private:
    d3d12::Mesh mesh_;
};

class SampleScene : public IScene {
public:
    static constexpr std::size_t meshIdx = 0;

    SampleScene(const SampleDrawable& drawable)
        : pDrawable_(&drawable), drawn_(false) {}

    std::optional<const DrawInfo> getDrawInfo() const override;

private:
    void buildRes(d3d12::Core& core, d3d12::D3D12RenderContext& ctx);

    const SampleDrawable* pDrawable_;
    mutable bool drawn_;
};

} // namespace gfx

#endif // __SAMPLE_SCENE_HPP