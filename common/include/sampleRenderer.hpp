#ifndef __SAMPLE_RENDERER_HPP
#define __SAMPLE_RENDERER_HPP

#include "d3d12core.hpp"
#include "rootPresets.hpp"

#include <string>

namespace gfx {

class SampleRenderer : public IRenderer {
public:
    static std::string rootName() {
        return d3d12::rootName(d3d12::RootPreset::Null);
    }

    static constexpr std::string shaderName() {
        return "sampleShader";
    }

    void init(ICore& core) override;
    void render(const IScene& scene, IRenderContext& renderContext, IRenderTarget& target) const override;
    void cleanup() override;

private:
    struct D3D12Drawer;
};

struct SampleRenderer::D3D12Drawer {
    static void init( SampleRenderer& renderer, d3d12::Core& core );

    static void render( const IScene& scene, ID3D12GraphicsCommandList* pCmdList,
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle
    );
};

} // namespace gfx

#endif // __SAMPLE_RENDERER_HPP