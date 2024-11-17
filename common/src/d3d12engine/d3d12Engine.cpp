#include "d3d12engine/d3d12Engine.hpp"

namespace gfx {

namespace d3d12 {

Core::Core(dx::DXGIFactory& factory)
    : device_( getAvailableAdapter(factory, D3D_FEATURE_LEVEL_12_1), D3D_FEATURE_LEVEL_12_1 ),
    cmdQueue_( device_ ), cmdList_( device_ ),
    rtvHeap_(device_, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, initialRtvHeapSize),
    dsvHeap_(device_, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, initialDsvHeapSize),
    cbvSrvUavHeap_(device_, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, initialCbvSrvUavHeapSize),
    descRanges_(rtvHeap_, dsvHeap_, cbvSrvUavHeap_),
    window_(), fence_(device_) {
    window_.open( factory, device_, cmdQueue_,
        descRanges_.rtvRangeBackBuf, descRanges_.dsvRangeBackBuf
    );
}

void Core::render(IRenderer& renderer) {
    cmdList_.reset();
    renderer.render(*this);
    cmdList_.close();
    cmdQueue_.execute(cmdList_);
    window_.present(cmdList_);
    fence_.signal(cmdQueue_);
    fence_.wait();
}

}   // namespace gfx::d3d12

}   // namespace gfx