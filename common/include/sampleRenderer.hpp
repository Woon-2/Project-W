#ifndef __SAMPLE_RENDERER_HPP
#define __SAMPLE_RENDERER_HPP

#include "rootPresets.hpp"

#include "renderProtocol.hpp"

#include "d3d12core.hpp"
#include "d3d12res.hpp"
#include "shaderRes.hpp"

#include <string>

namespace gfx {

class SampleRenderer : public IRenderer {
public:
    void init(ICore& core) override;
    void render(const IScene& scene, IRenderContext& renderContext, IRenderTarget& target) const override;
    void cleanup() override;

    rp::Protocol protocol() const override {
        return protocol_;
    }

private:
    class D3D12Drawer {
    public:
        static constexpr std::size_t maxInstances = 1000u;
        static constexpr std::size_t maxLights = 100u;
        static constexpr std::size_t maxMaterials = 100u;

        void init( SampleRenderer& renderer, d3d12::Core& core );
        void cleanup() {}
        void phongInstancingNT( const IScene& scene, d3d12::Shader& shader, ID3D12GraphicsCommandList* pCmdList,
            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle
        ) const;
    };

    D3D12Drawer drawer_;
    rp::Protocol protocol_;
};

} // namespace gfx

#endif // __SAMPLE_RENDERER_HPP