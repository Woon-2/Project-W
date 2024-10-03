#ifndef __D3D12MESH_HPP
#define __D3D12MESH_HPP

#include "mesh.hpp"

#include "d3d12core.hpp"

#include "gfxPrimitive.hpp"

#include <ranges>
#include <utility>

namespace gfx {

namespace d3d12 {

class Mesh : public gfx::Mesh {
public:
    Mesh() = default;

    template < std::ranges::range VBS, std::ranges::range IB,
        class VBIt = std::conditional_t<
            std::is_lvalue_reference_v< std::remove_const_t<VBS> >,
            typename VBS::const_iterator,
            std::move_iterator<typename VBS::iterator> 
        >
    > requires std::same_as<std::ranges::range_value_t<VBS>, VertexBuffer>
            && std::same_as<std::ranges::range_value_t<IB>, Index>
    Mesh(Core& core, D3D12RenderContext& ctx, VBS&& vbs, IB&& ib)
        : gfx::Mesh(std::forward<VBS>(vbs), std::forward<IB>(ib)),
        vbvs_(), ibv_(), vbrs_(), ibr_(), vubs_(), iub_() {
        buildRes(core, ctx);
    }

    template < std::ranges::range VBS,
        class VBIt = std::conditional_t<
            std::is_lvalue_reference_v< std::remove_const_t<VBS> >,
            typename VBS::const_iterator,
            std::move_iterator<typename VBS::iterator> 
        >
    > requires std::same_as<std::ranges::range_value_t<VBS>, VertexBuffer>
    Mesh(Core& core, D3D12RenderContext& ctx, VBS&& vbs, Cont<Index>&& ib)
        : gfx::Mesh(std::forward<VBS>(vbs), std::move(ib)),
        vbvs_(), ibv_(), vbrs_(), ibr_(), vubs_(), iub_() {
        buildRes(core, ctx);
    }

    template <std::ranges::range IB>
        requires std::same_as<std::ranges::range_value_t<IB>, Index>
    Mesh(Core& core, D3D12RenderContext& ctx, Cont<VertexBuffer>&& vbs, IB&& ib)
        : gfx::Mesh(std::move(vbs), std::forward<IB>(ib)),
        vbvs_(), ibv_(), vbrs_(), ibr_(), vubs_(), iub_() {
        buildRes(core, ctx);
    }

    Mesh(Core& core, D3D12RenderContext& ctx, Cont<VertexBuffer>&& vbs, Cont<Index>&& ib)
        : gfx::Mesh(std::move(vbs), std::move(ib)),
        vbvs_(), ibv_(), vbrs_(), ibr_(), vubs_(), iub_() {
        buildRes(core, ctx);
    }

    Mesh(const Mesh& mesh)
        : gfx::Mesh(mesh), vbvs_(mesh.vbvs_), ibv_(mesh.ibv_),
        vbrs_(mesh.vbrs_), ibr_(mesh.ibr_), vubs_(), iub_() {}

    Mesh(Mesh&& mesh) noexcept
        : gfx::Mesh(std::move(mesh)), vbvs_(std::exchange(mesh.vbvs_, {})), ibv_(std::exchange(mesh.ibv_, {})),
        vbrs_(std::move(mesh.vbrs_)), ibr_(std::move(mesh.ibr_)), vubs_(std::move(mesh.vubs_)), iub_(std::move(mesh.iub_)) {}

    Mesh& operator=(const Mesh& mesh) {
        if (this == &mesh) {
            return *this;
        }

        Mesh::operator=(mesh);

        vbvs_ = mesh.vbvs_;
        ibv_ = mesh.ibv_;
        vbrs_ = mesh.vbrs_;
        ibr_ = mesh.ibr_;

        return *this;
    }

    Mesh& operator=(Mesh&& mesh) noexcept {
        if (this == &mesh) {
            return *this;
        }

        Mesh::operator=(std::move(mesh));

        vbvs_ = std::exchange(mesh.vbvs_, {});
        ibv_ = std::exchange(mesh.ibv_, {});
        vbrs_ = std::move(mesh.vbrs_);
        ibr_ = std::move(mesh.ibr_);
        vubs_ = std::move(mesh.vubs_);
        iub_ = std::move(mesh.iub_);

        return *this;
    }

    void completeInit();
    void bind(d3d12::D3D12RenderContext& ctx) const;
    void draw(d3d12::D3D12RenderContext& ctx) const;
    void draw(d3d12::D3D12RenderContext& ctx, std::size_t instanceCount) const;

private:
    void buildRes(d3d12::Core& core, d3d12::D3D12RenderContext& ctx);

    Cont<D3D12_VERTEX_BUFFER_VIEW> vbvs_;
    D3D12_INDEX_BUFFER_VIEW ibv_;
    Cont<wrl::ComPtr<ID3D12Resource>> vbrs_;
    wrl::ComPtr<ID3D12Resource> ibr_;
    Cont<wrl::ComPtr<ID3D12Resource>> vubs_;
    wrl::ComPtr<ID3D12Resource> iub_;
};

}   // namespace d3d12

}   // namespace gfx

#endif // __D3D12MESH_HPP