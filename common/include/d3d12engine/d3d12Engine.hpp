#ifndef __D3D12Engine_HPP
#define __D3D12Engine_HPP

#include "d3d12engine/descriptorRangeSpec.hpp"

#include "d3d12util/d3d12Low.hpp"
#include "d3d12util/d3d12RenderPass.hpp"
#include "d3d12util/d3d12ResourceXX.hpp"
#include "d3d12util/d3d12ShaderXX.hpp"

namespace gfx {

namespace d3d12engine {

inline constexpr auto initialRtvHeapSize = 3u;
inline constexpr auto initialDsvHeapSize = 1u;
inline constexpr auto initialCbvSrvUavHeapSize = 1000u;

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void render(class Core&) = 0;
};

class Core {
public:
    using MyWindow = d3d12::Window<d3d12::BasicD3D12WTraits<char>>;

    Core(dx::DXGIFactory& factory);

    d3d12::D3D12GfxCmdList fetchCmdList() {
        return cmdList_;
    }

    void render(IRenderer& renderer);

private:
    d3d12::D3D12Device device_;
    d3d12::D3D12CmdQueue cmdQueue_;
    d3d12::D3D12GfxCmdList cmdList_;
    d3d12::DescriptorHeapCPU rtvHeap_;
    d3d12::DescriptorHeapCPU dsvHeap_;
    d3d12::DescriptorHeapGPU cbvSrvUavHeap_;
    d3d12::DescriptorRanges descRanges_;
    MyWindow window_;
    d3d12::Fence fence_;
};

class Scene {
public:

private:
};

}   // namespace gfx::d3d12engine

}   // namespace gfx

#endif  // __D3D12Engine_HPP