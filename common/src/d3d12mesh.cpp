#include "d3d12mesh.hpp"

#include "d3d12res.hpp"

namespace gfx {

namespace d3d12 {

void Mesh::completeInit() {
    vubs_.clear();
    iub_.Reset();
}

void Mesh::bind(D3D12RenderContext& ctx) const {
    auto cmdList = std::any_cast<wrl::ComPtr<ID3D12GraphicsCommandList>>(
        ctx.cast(gfx::RenderContextType::D3D12)
    );

    DX_THROW_FAILED_VOID(
        cmdList->IASetVertexBuffers(0, static_cast<UINT>( vbvs_.size() ), vbvs_.data())
    );
    DX_THROW_FAILED_VOID( cmdList->IASetIndexBuffer(&ibv_) );
}

void Mesh::draw(D3D12RenderContext& ctx) const {
    auto cmdList = std::any_cast<wrl::ComPtr<ID3D12GraphicsCommandList>>(
        ctx.cast(gfx::RenderContextType::D3D12)
    );

    DX_THROW_FAILED_VOID( cmdList->DrawIndexedInstanced(
        static_cast<UINT>( ibv_.SizeInBytes / sizeof(Index) ), 1, 0, 0, 0
    ) );
}

void Mesh::draw(D3D12RenderContext& ctx, std::size_t instanceCount) const {
    auto cmdList = std::any_cast<wrl::ComPtr<ID3D12GraphicsCommandList>>(
        ctx.cast(gfx::RenderContextType::D3D12)
    );

    DX_THROW_FAILED_VOID( cmdList->DrawIndexedInstanced(
        static_cast<UINT>( ibv_.SizeInBytes / sizeof(Index) ),
        static_cast<UINT>( instanceCount ), 0, 0, 0
    ) );
}

void Mesh::buildRes(Core& core, D3D12RenderContext& ctx) {
    // Vertex buffers
    vubs_.clear();
    vbvs_.clear();
    for (const auto& vb : vbs()) {
        auto vub = createUpBuf(core, vb.rawMem(), vb.byteWidth());
        auto vbr = createDefBuf(core, ctx, vub.Get(), D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        vubs_.push_back(vub);
        vbrs_.push_back(vbr);
        vbvs_.emplace_back( 
            /* .BufferLocation = */ vub->GetGPUVirtualAddress(),
            /* .SizeInBytes = */ static_cast<UINT>( vb.byteWidth() ),
            /* .StrideInBytes = */ static_cast<UINT>( vb.stride() )
        );
    }

    // Index buffer
    const auto ibByteWidth = ib().size() * sizeof(Index);
    iub_ = createUpBuf(core, ib().data(), ibByteWidth);
    ibv_.BufferLocation = iub_->GetGPUVirtualAddress();
    ibv_.SizeInBytes = static_cast<UINT>( ibByteWidth );
    ibv_.Format = DXGI_FORMAT_R32_UINT;
}

}   // namespace d3d12

}   // namespace gfx