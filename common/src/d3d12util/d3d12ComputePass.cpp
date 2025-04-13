#include "d3d12util/d3d12ComputePass.hpp"

namespace gfx {

namespace d3d12 {

namespace cp {

MatMul::MatMul(D3D12Device& device, ShaderMatMul& shader)
    : ComputePass(id), protocol_(shader.makeProtocol(device, makeDesc())),
      threadGroupCnt_(0u) {}

ComputeProtocol::Desc MatMul::makeDesc() {
    return ComputeProtocol::Desc{
        .nodeMask = 0u
    };
}

void MatMul::setThreadCnt(std::size_t threadCnt) {
    threadGroupCnt_ = (threadCnt + ShaderMatMul::groupSizeX - 1) / ShaderMatMul::groupSizeX;
    if (threadGroupCnt_ > shader().maxMatrixCnt()) {
        throw GFX_EXCEPT("[Description] Matmul::setThreadCnt: Thread count exceeds max matrix count.");
    }
}

void MatMul::preCompute(D3D12GfxCmdList& cmdList) {
    cmdList.get()->SetPipelineState(protocol_.get().Get());
}

void MatMul::compute(D3D12GfxCmdList& cmdList) {
    cmdList.get()->Dispatch(static_cast<UINT>(threadGroupCnt_), 1u, 1u);
}

// must wait for the compute shader to finish before reading back the result
void MatMul::postCompute(D3D12GfxCmdList& cmdList) {
    shader().resultMatrices_.readback(cmdList, shader().resultMatricesSrc_);
    resultMatrices_.reserve(shader().maxMatrixCnt());
    
    auto pMatrices = shader().resultMatrices_.getReadbackResult<dx::XMFLOAT4X4>();
    for (std::size_t i = 0u; i < threadGroupCnt_ * shader().groupSizeX; ++i) {
        resultMatrices_.emplace_back(dx::loadMat(pMatrices[i]));
    }

}


}   // namespace gfx::d3d12::cp

}   // namespace gfx::d3d12

}   // namespace gfx