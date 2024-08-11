#ifndef __D3D12MESH_HPP
#define __D3D12MESH_HPP

#include "mesh.hpp"

#include "d3d12core.hpp"

#include "gfxPrimitive.hpp"

#include <ranges>

namespace gfx {

namespace d3d12 {

class Mesh {
public:
    using IndexCont = gfx::Mesh::IndexCont;

    Mesh( d3d12::Core& core, d3d12::D3D12RenderContext& ctx, const VertexBuffer& vbuf, const IndexCont& ibuf,
        Core::UpBufIdx vbUpIdx, Core::UpBufIdx ibUpIdx
    ) : vbView_(), ibView_(), vb_(), ib_(),
        vbUpIdx_(vbUpIdx), ibUpIdx_(ibUpIdx) {
        buildRes(core, ctx, vbuf, ibuf);
    }

    template <std::ranges::range R>
    Mesh( d3d12::Core& core, d3d12::D3D12RenderContext& ctx, const VertexBuffer& vb, R&& ib,
        Core::UpBufIdx vbUpIdx, Core::UpBufIdx ibUpIdx
    ) : vbView_(), ibView_(), vb_(), ib_(),
        vbUpIdx_(vbUpIdx), ibUpIdx_(ibUpIdx) {
        buildRes(core, ctx, vb, IndexCont(std::begin(ib), std::end(ib)));
    }

    Mesh( d3d12::Core& core, d3d12::D3D12RenderContext& ctx, const gfx::Mesh& mesh,
        Core::UpBufIdx vbUpIdx, Core::UpBufIdx ibUpIdx
    ) : vbView_(), ibView_(), vb_(), ib_(),
        vbUpIdx_(vbUpIdx), ibUpIdx_(ibUpIdx) {
        buildRes(core, ctx, mesh.vb(), mesh.ib());
    }

    void completeInit(d3d12::Core& core);
    void bind(ID3D12GraphicsCommandList* pCmdList) const;
    void draw(ID3D12GraphicsCommandList* pCmdList) const;

private:
    void buildRes(d3d12::Core& core, d3d12::D3D12RenderContext& ctx, const VertexBuffer& vb, const IndexCont& ib);

    std::array<D3D12_VERTEX_BUFFER_VIEW, 1> vbView_;
    D3D12_INDEX_BUFFER_VIEW ibView_;
    wrl::ComPtr<ID3D12Resource> vb_;
    wrl::ComPtr<ID3D12Resource> ib_;
    Core::UpBufIdx vbUpIdx_;
    Core::UpBufIdx ibUpIdx_;
};

}   // namespace d3d12

}   // namespace gfx

#endif // __D3D12MESH_HPP