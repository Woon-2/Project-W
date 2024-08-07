#ifndef __SAMPLE_RENDERER_HPP
#define __SAMPLE_RENDERER_HPP

#include "gfx.hpp"

#include <d3d12.h>
#include "dxtarget.hpp"

namespace gfx {

class SampleRenderer : public IRenderer {
public:
    void init() override;
    void render(const IScene& scene, IRenderContext& renderContext, IRenderTarget& target) const override;
    void cleanup() override;

private:
    struct D3D12Drawer;
};

struct SampleRenderer::D3D12Drawer {
    static void render( const IScene& scene, ID3D12GraphicsCommandList* pCmdList,
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle
    );
};

} // namespace gfx

#endif // __SAMPLE_RENDERER_HPP