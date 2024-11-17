#ifndef __descriptorRangeSpec_HPP
#define __descriptorRangeSpec_HPP

#include "d3d12util/d3d12Low.hpp"

namespace gfx {

namespace d3d12 {

struct DescriptorRanges {
    DescriptorRanges(
        DescriptorHeapCPU& rtvHeap, DescriptorHeapCPU& dsvHeap,
        DescriptorHeapGPU& cbvSrvUavHeap
    ) : rtvRangeBackBuf(rtvHeap, 0, 3, DescriptorCPU::Type::RTV),
        dsvRangeBackBuf(dsvHeap, 0, 1, DescriptorCPU::Type::DSV),
        srvRangeTex2D(cbvSrvUavHeap, 0, 200, DescriptorGPU::Type::SRV),
        srvRangeTex2DArray(cbvSrvUavHeap, 200, 400, DescriptorGPU::Type::SRV),
        srvRangeTexCube(cbvSrvUavHeap, 400, 600, DescriptorGPU::Type::SRV) {}

    DescriptorRange<DescriptorHeapCPU> rtvRangeBackBuf;
    DescriptorRange<DescriptorHeapCPU> dsvRangeBackBuf;
    DescriptorRange<DescriptorHeapGPU> srvRangeTex2D;
    DescriptorRange<DescriptorHeapGPU> srvRangeTex2DArray;
    DescriptorRange<DescriptorHeapGPU> srvRangeTexCube;
};

}   // namespace gfx::d3d12

}   // namespace gfx

#endif  // __descriptorRangeSpec_HPP