#ifndef __SAMPLE_SCENE_HPP
#define __SAMPLE_SCENE_HPP

#include "d3d12core.hpp"

#include "gfxPrimitive.hpp"

#include <array>

namespace gfx {

class SampleDrawable {
public:
    static constexpr std::size_t vbIdx = 0;
    static constexpr std::size_t ibIdx = 1;

    SampleDrawable(d3d12::Core& core, d3d12::D3D12RenderContext& ctx)
        : vbView_({}), ibView_({}) {
        buildRes(core, ctx);
    }

    void completeInit(d3d12::Core& core);

    const D3D12_VERTEX_BUFFER_VIEW vbView() const NOEXCEPT {
        return vbView_[0];
    }

    const D3D12_INDEX_BUFFER_VIEW ibView() const NOEXCEPT {
        return ibView_;
    }

private:
    void buildRes(d3d12::Core& core, d3d12::D3D12RenderContext& ctx);

    std::array<D3D12_VERTEX_BUFFER_VIEW, 1> vbView_;
    D3D12_INDEX_BUFFER_VIEW ibView_;
    wrl::ComPtr<ID3D12Resource> vb_;
    wrl::ComPtr<ID3D12Resource> ib_;
};

class SampleScene : public IScene {
public:
    static constexpr std::size_t vbIdx = 0;
    static constexpr std::size_t ibIdx = 1;

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