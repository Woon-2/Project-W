#include "d3d12mesh.hpp"

#include "d3d12res.hpp"

namespace gfx {

namespace d3d12 {

void Mesh::completeInit(Core& core) {
    core.popTmpUpBuf(vbUpIdx_);
    core.popTmpUpBuf(ibUpIdx_);
}

void Mesh::bind(ID3D12GraphicsCommandList* pCmdList) const {
    DX_THROW_FAILED_VOID( pCmdList->IASetVertexBuffers(0, 1, vbView_.data()) );
    DX_THROW_FAILED_VOID( pCmdList->IASetIndexBuffer(&ibView_) );
}

void Mesh::draw(ID3D12GraphicsCommandList* pCmdList) const {
    DX_THROW_FAILED_VOID( pCmdList->DrawIndexedInstanced(
        static_cast<UINT>( ib().size() ), 1, 0, 0, 0
    ) );
}

void Mesh::buildRes(Core& core, D3D12RenderContext& ctx) {
    core.addTmpUpBuf(vbUpIdx_);
    core.addTmpUpBuf(ibUpIdx_);
    // Vertex buffer
    vb_ = d3d12::createDefBuf( core, ctx, vb().rawMem(), static_cast<std::uint32_t>( vb().byteWidth() ),
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, core.tmpUpBuf(vbUpIdx_)
    );
    vbView_[0] = D3D12_VERTEX_BUFFER_VIEW {
        .BufferLocation = vb_->GetGPUVirtualAddress(),
        .SizeInBytes = static_cast<UINT>( vb().byteWidth() ),
        .StrideInBytes = static_cast<UINT>( vb().stride() )
    };

    // Index buffer
    ib_ = d3d12::createDefBuf(core, ctx, ib(), D3D12_RESOURCE_STATE_INDEX_BUFFER, core.tmpUpBuf(ibUpIdx_));
    ibView_ = D3D12_INDEX_BUFFER_VIEW {
        .BufferLocation = ib_->GetGPUVirtualAddress(),
        .SizeInBytes = static_cast<std::uint32_t>( ib().size() * sizeof(IndexCont::value_type) ),
        .Format = DXGI_FORMAT_R32_UINT
    };
}

}   // namespace d3d12

}   // namespace gfx