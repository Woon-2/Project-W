#include "d3d12mesh.hpp"

#include "d3d12res.hpp"

namespace gfx {

namespace d3d12 {

void Mesh::completeInit(Core& core) {
    core.popTmpUpBuf("SampleTriangleVB");
    core.popTmpUpBuf("SampleTriangleIB");
}

void Mesh::bind(D3D12RenderContext& ctx) const NOEXCEPT {
    auto pCmdList = std::any_cast<wrl::ComPtr<ID3D12GraphicsCommandList>>(
        ctx.cast(RenderContextType::D3D12)
    );

    pCmdList->IASetVertexBuffers(0, 1, vbView_.data());
    pCmdList->IASetIndexBuffer(&ibView_);
}

void Mesh::buildRes(Core& core, D3D12RenderContext& ctx) {
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

}   // namespace d3d12

}   // namespace gfx