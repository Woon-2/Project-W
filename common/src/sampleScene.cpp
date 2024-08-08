#include "SampleScene.hpp"

#include "d3d12res.hpp"

namespace gfx {

void SampleDrawable::completeInit(d3d12::Core& core) {
    core.popTmpUpBuf("SampleTriangleVB");
    core.popTmpUpBuf("SampleTriangleIB");
}

void SampleDrawable::buildRes(d3d12::Core& core, d3d12::D3D12RenderContext& ctx) {
    core.addTmpUpBuf("SampleTriangleVB");
    core.addTmpUpBuf("SampleTriangleIB");
    // Vertex buffer
    std::array<Position, 3> vertices = {
        Position{0.0f, 0.5f, 0.0f},
        Position{0.5f, -0.5f, 0.0f},
        Position{-0.5f, -0.5f, 0.0f}
    };
    vb_ = d3d12::createDefBuf(core, ctx, vertices, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, core.tmpUpBuf("SampleTriangleVB"));
    vbView_[0] = D3D12_VERTEX_BUFFER_VIEW {
        .BufferLocation = vb_->GetGPUVirtualAddress(),
        .SizeInBytes = static_cast<UINT>( sizeof(Position) * vertices.size() ),
        .StrideInBytes = static_cast<UINT>( sizeof(Position) )
    };

    // Index buffer
    std::array<UINT, 3> indices = {0, 1, 2};
    ib_ = d3d12::createDefBuf(core, ctx, indices, D3D12_RESOURCE_STATE_INDEX_BUFFER, core.tmpUpBuf("SampleTriangleIB"));
    ibView_ = D3D12_INDEX_BUFFER_VIEW {
        .BufferLocation = ib_->GetGPUVirtualAddress(),
        .SizeInBytes = static_cast<UINT>( sizeof(UINT) * indices.size() ),
        .Format = DXGI_FORMAT_R32_UINT
    };
}

std::optional<const DrawInfo> SampleScene::getDrawInfo() const {
    if (drawn_) {
        return {};
    }

    auto ret = DrawInfo();

    ret.set(vbIdx, pDrawable_->vbView());
    ret.set(ibIdx, pDrawable_->ibView());

    drawn_ = true;

    return ret;
}

}   // namespace gfx