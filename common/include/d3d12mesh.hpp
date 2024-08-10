#ifndef __D3D12MESH_HPP
#define __D3D12MESH_HPP

#include "mesh.hpp"

#include "d3d12core.hpp"

#include "gfxPrimitive.hpp"

#include <ranges>

namespace gfx {

namespace d3d12 {

class Mesh : public gfx::Mesh {
public:
    Mesh(d3d12::Core& core, d3d12::D3D12RenderContext& ctx, VertexBuffer vb, IndexCont&& ib)
        : gfx::Mesh(std::move(vb), std::move(ib)), vbView_(), ibView_(), vb_(), ib_() {
        buildRes(core, ctx);
    }

    template <std::ranges::range R>
    Mesh(d3d12::Core& core, d3d12::D3D12RenderContext& ctx, VertexBuffer vb, R&& ib)
        : gfx::Mesh(std::move(vb), std::forward<R>(ib)), vbView_(), ibView_(), vb_(), ib_() {
        buildRes(core, ctx);
    }

    void completeInit(d3d12::Core& core);
    void bind(d3d12::D3D12RenderContext& ctx) const NOEXCEPT;

private:
    void buildRes(d3d12::Core& core, d3d12::D3D12RenderContext& ctx);

    std::array<D3D12_VERTEX_BUFFER_VIEW, 1> vbView_;
    D3D12_INDEX_BUFFER_VIEW ibView_;
    wrl::ComPtr<ID3D12Resource> vb_;
    wrl::ComPtr<ID3D12Resource> ib_;
};

}   // namespace d3d12

}   // namespace gfx

#endif // __D3D12MESH_HPP