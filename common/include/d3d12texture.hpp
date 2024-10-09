#ifndef __D3d12Texture_HPP
#define __D3d12Texture_HPP

#include "d3d12core.hpp"

#include "texLoader/DDSTextureLoader12.h"
#include "texLoader/WICTextureLoader12.h"

#include "d3d12resLow.hpp"

#include <filesystem>

namespace gfx {

namespace d3d12 {

class Texture {
public:
    Texture() NOEXCEPT
        : desc_{}, srv_(), rtv_(), dsv_(), res_(), upRes_(), state_(D3D12_RESOURCE_STATE_COMMON) {}

    Texture( Core& core, const D3D12_RESOURCE_DESC& desc,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
    ) : desc_(desc), srv_(), res_(), upRes_(), state_(initialState) {
        res_ = createDefRes(core, desc, initialState);
    }

    Texture( Core& core, const D3D12_RESOURCE_DESC& desc, const D3D12_CLEAR_VALUE& clearValue,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
    ) : desc_(desc), srv_(), res_(), upRes_(), state_(initialState) {
        res_ = createDefRes(core, desc, initialState, clearValue);
    }

    Texture( Core& core, D3D12RenderContext& ctx, const std::filesystem::path& path,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
    ) : Texture() {
        load(core, ctx, path, initialState);
    }

    void load( Core& core, D3D12RenderContext& ctx, const std::filesystem::path& path,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
    );

    void completeInit() {
        upRes_.Reset();
    }

    void makeSrv(Core& core, const Descriptor& desc);
    void makeSrv(Core& core, const Descriptor& desc, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc);
    void makeRtv(Core& core, const Descriptor& desc);
    void makeDsv(Core& core, const Descriptor& desc);

    const Descriptor& srv() const NOEXCEPT {
        return srv_;
    }
    const Descriptor& rtv() const NOEXCEPT {
        return rtv_;
    }
    const Descriptor& dsv() const NOEXCEPT {
        return dsv_;
    }

    void cvt2Common( D3D12RenderContext& ctx ) {
        cvt(ctx, D3D12_RESOURCE_STATE_COMMON);
    }
    void cvt2rt( D3D12RenderContext& ctx ) {
        cvt(ctx, D3D12_RESOURCE_STATE_RENDER_TARGET);
    }
    void cvt2ds( D3D12RenderContext& ctx ) {
        cvt(ctx, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    }
    void cvt2sr( D3D12RenderContext& ctx ) {
        cvt(ctx, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    }
    void cvt2cpSrc( D3D12RenderContext& ctx ) {
        cvt(ctx, D3D12_RESOURCE_STATE_COPY_SOURCE);
    }
    void cvt2cpDst( D3D12RenderContext& ctx ) {
        cvt(ctx, D3D12_RESOURCE_STATE_COPY_DEST);
    }

    D3D12_RESOURCE_STATES state() const NOEXCEPT {
        return state_;
    }

    const D3D12_RESOURCE_DESC& resDesc() const NOEXCEPT {
        return desc_;
    }

private:
    void cvt( D3D12RenderContext& ctx, D3D12_RESOURCE_STATES state );

    void loadDDS( Core& core, D3D12RenderContext& ctx, const std::filesystem::path& path,
        D3D12_RESOURCE_STATES initialState
    );
    void loadWIC( Core& core, D3D12RenderContext& ctx, const std::filesystem::path& path,
        D3D12_RESOURCE_STATES initialState
    );
    
    D3D12_RESOURCE_DESC desc_;
    Descriptor srv_;
    Descriptor rtv_;
    Descriptor dsv_;
    wrl::ComPtr<ID3D12Resource> res_;
    wrl::ComPtr<ID3D12Resource> upRes_;
    D3D12_RESOURCE_STATES state_;
};

}   // namespace gfx::d3d12

}   // namespace gfx

#endif // __D3d12Texture_HPP