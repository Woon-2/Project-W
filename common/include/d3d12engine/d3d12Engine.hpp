#ifndef __D3D12Engine_HPP
#define __D3D12Engine_HPP

#include "d3d12engine/descriptorRangeSpec.hpp"

#include "d3d12util/d3d12Low.hpp"
#include "d3d12util/d3d12RenderPass.hpp"
#include "d3d12util/d3d12ResourceXX.hpp"
#include "d3d12util/d3d12ShaderXX.hpp"

namespace gfx {

namespace d3d12 {

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
    using MyWindow = Window<BasicD3D12WTraits<char>>;

    Core(dx::DXGIFactory& factory);

    D3D12GfxCmdList fetchCmdList() {
        return cmdList_;
    }

    void render(IRenderer& renderer);

private:
    D3D12Device device_;
    D3D12CmdQueue cmdQueue_;
    D3D12GfxCmdList cmdList_;
    DescriptorHeapCPU rtvHeap_;
    DescriptorHeapCPU dsvHeap_;
    DescriptorHeapGPU cbvSrvUavHeap_;
    DescriptorRanges descRanges_;
    MyWindow window_;
    Fence fence_;
};

}   // namespace gfx::d3d12

}   // namespace gfx

#endif  // __D3D12Engine_HPP