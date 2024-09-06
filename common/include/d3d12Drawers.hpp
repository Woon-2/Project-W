#ifndef __D3d12Drawers_HPP
#define __D3d12Drawers_HPP

#include "d3d12core.hpp"
#include "phongShader.hpp"

namespace gfx {

namespace d3d12 {

void illuminanceDraw(const IScene& scene, rp::PhongInstancing protocol, PhongShader& shader, ID3D12GraphicsCommandList* pCmdList);
void illuminanceDraw(const IScene& scene, rp::PhongInstancingNT protocol, PhongShaderNT& shader, ID3D12GraphicsCommandList* pCmdList);

}   // namespace gfx::d3d12

}   // namespace gfx

#endif // __D3d12Drawers_HPP