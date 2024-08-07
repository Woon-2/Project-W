#ifndef __D3D12RES_HPP
#define __D3D12RES_HPP

#include "d3d12core.hpp"

namespace gfx {

namespace d3d12 {

template <std::ranges::contiguous_range R>
    requires std::ranges::sized_range<R>
wrl::ComPtr<ID3D12Resource> createUpBuf(Core& core, const R& data) {
    return createUpBuf( core, std::data(data), std::size(data)
        * sizeof(std::ranges::range_value_t<R>)
    );
}

template <std::ranges::contiguous_range R>
    requires std::ranges::sized_range<R>
wrl::ComPtr<ID3D12Resource> createDefBuf( Core& core, D3D12RenderContext& ctx, const R& data,
    D3D12_RESOURCE_STATES state, wrl::ComPtr<ID3D12Resource>& pUploadBuf
) {
    return createDefBuf( core, ctx, std::data(data), static_cast<UINT>( std::size(data) )
        * sizeof(std::ranges::range_value_t<R>), state, pUploadBuf
    );
}

wrl::ComPtr<ID3D12Resource> createUpBuf(Core& core, UINT bytes);
wrl::ComPtr<ID3D12Resource> createUpBuf(Core& core, const void* pData, UINT bytes);
wrl::ComPtr<ID3D12Resource> createDefBuf(Core& core, D3D12RenderContext& ctx, ID3D12Resource* pSrcBuf, D3D12_RESOURCE_STATES state);
wrl::ComPtr<ID3D12Resource> createDefBuf(Core& core, D3D12RenderContext& ctx, const void* pData, UINT bytes, D3D12_RESOURCE_STATES state, wrl::ComPtr<ID3D12Resource>& pUploadBuf);

}   // namespace d3d12

} // namespace gfx

#endif // __D3D12RES_HPP