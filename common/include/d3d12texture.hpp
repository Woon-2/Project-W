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
        : desc_{}, srv_(), rtv_(), dsv_(), res_(), upRes_() {}

    Texture( Core& core, const D3D12_RESOURCE_DESC& desc,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE
    ) : desc_(desc), srv_(), res_(), upRes_() {
        res_ = createDefRes(core, desc, initialState);
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

private:
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
};

}   // namespace gfx::d3d12

}   // namespace gfx

#endif // __D3d12Texture_HPP