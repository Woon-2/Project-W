#include "d3d12mesh.hpp"

#include "d3d12res.hpp"

namespace gfx {

namespace d3d12 {

void Mesh::completeInit() {
    vub_.Reset();
    iub_.Reset();
}

void Mesh::bind(ID3D12GraphicsCommandList* pCmdList) const {
    DX_THROW_FAILED_VOID( pCmdList->IASetVertexBuffers(0, 1, vbView_.data()) );
    DX_THROW_FAILED_VOID( pCmdList->IASetIndexBuffer(&ibView_) );
}

void Mesh::draw(ID3D12GraphicsCommandList* pCmdList) const {
    DX_THROW_FAILED_VOID( pCmdList->DrawIndexedInstanced(
        static_cast<UINT>( ibView_.SizeInBytes / sizeof(IndexCont::value_type) ), 1, 0, 0, 0
    ) );
}

void Mesh::draw(ID3D12GraphicsCommandList* pCmdList, std::size_t instanceCount) const {
    DX_THROW_FAILED_VOID( pCmdList->DrawIndexedInstanced(
        static_cast<UINT>( ibView_.SizeInBytes / sizeof(IndexCont::value_type) ),
        static_cast<UINT>( instanceCount ), 0, 0, 0
    ) );
}

void Mesh::buildRes(Core& core, D3D12RenderContext& ctx, const VertexBuffer& vbuf, const IndexCont& ibuf) {
    // Vertex buffer
    vub_ = createUpBuf( core, vbuf.rawMem(), vbuf.byteWidth() );
    vb_ = createDefBuf( core, ctx, vub_.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER );
    vbView_[0] = D3D12_VERTEX_BUFFER_VIEW {
        .BufferLocation = vb_->GetGPUVirtualAddress(),
        .SizeInBytes = static_cast<UINT>( vbuf.byteWidth() ),
        .StrideInBytes = static_cast<UINT>( vbuf.stride() )
    };

    // Index buffer
    const auto ibByteWidth = ibuf.size() * sizeof(IndexCont::value_type);
    iub_ = createUpBuf( core, ibuf.data(), ibByteWidth );
    ib_ = createDefBuf( core, ctx, iub_.Get(), D3D12_RESOURCE_STATE_INDEX_BUFFER );
    ibView_ = D3D12_INDEX_BUFFER_VIEW {
        .BufferLocation = ib_->GetGPUVirtualAddress(),
        .SizeInBytes = static_cast<std::uint32_t>( ibByteWidth ),
        .Format = DXGI_FORMAT_R32_UINT
    };
}

}   // namespace d3d12

}   // namespace gfx