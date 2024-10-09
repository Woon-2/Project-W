#ifndef __D3d12Drawers_HPP
#define __D3d12Drawers_HPP

#include "d3d12core.hpp"
#include "phongShader.hpp"

namespace gfx {

namespace d3d12 {

template <class T>
concept PFUnifiedD3d12Shader = rp::PFUnifiedShader<T>
    && requires (T& t, ID3D12GraphicsCommandList* pCmdList) {
        t.setRootParams(pCmdList);
    };

void illuminanceDraw(const IScene& scene, rp::Protocol protocol, Shader& shader, D3D12RenderContext& ctx);

}   // namespace gfx::d3d12

}   // namespace gfx

#endif // __D3d12Drawers_HPP