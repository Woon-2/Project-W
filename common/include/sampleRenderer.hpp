#ifndef __SAMPLE_RENDERER_HPP
#define __SAMPLE_RENDERER_HPP

#include "rootPresets.hpp"

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

private:
    class D3D12Drawer {
    public:
        static constexpr std::size_t maxInstances = 1000u;
        static constexpr std::size_t maxLights = 100u;
        static constexpr std::size_t maxMaterials = 100u;

        void init( SampleRenderer& renderer, d3d12::Core& core );
        void cleanup();
        void render( const IScene& scene, ID3D12GraphicsCommandList* pCmdList,
            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle
        ) const;

    private:
        wrl::ComPtr<ID3D12Resource> resPerFrameData_;
        wrl::ComPtr<ID3D12Resource> resPerDrawcallData_;
        wrl::ComPtr<ID3D12Resource> resPerInstanceData_;
        wrl::ComPtr<ID3D12Resource> resMaterials_;
        wrl::ComPtr<ID3D12Resource> resLights_;

        mutable d3d12::sr::BasicPFD* pPFD_;
        mutable d3d12::sr::BasicPDD* pPDD_;
        mutable d3d12::sr::BasicPID* pPID_;
        mutable d3d12::sr::PhongMaterial* pMats_;
        mutable d3d12::sr::PhongLight* pLights_;
    };

    D3D12Drawer drawer_;

};

} // namespace gfx

#endif // __SAMPLE_RENDERER_HPP