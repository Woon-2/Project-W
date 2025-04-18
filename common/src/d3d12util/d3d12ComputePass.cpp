#include "d3d12util/d3d12ComputePass.hpp"

#include <ranges>
#include <algorithm>

namespace gfx {

namespace d3d12 {

namespace cp {

AnimInterpolation::AnimInterpolation(D3D12Device& device, ShaderAnimInterpolation& shader)
    : ComputePass(id), protocol_(shader.makeProtocol(device, makeDesc())),
      lhsKeyFrames_(), rhsKeyFrames_(), resultMatrices_(), threadGroupCnt_(0u) {
    checkRequiredTextures();
}

ComputeProtocol::Desc AnimInterpolation::makeDesc() {
    return ComputeProtocol::Desc{
        .nodeMask = 0u
    };
}

std::size_t AnimInterpolation::addKeyFramePair(const KeyFrame& lhs, const KeyFrame& rhs, float ratio) {
    lhsKeyFrames_.emplace_back(
        /* .pos = */ lhs.pos.getXmf(),
        /* .padding = */ 0.f,
        /* .scale = */ lhs.scale.getXmf(),
        /* .padding2 = */ 0.f,
        /* .rot = */ lhs.rot.getXmf(),
        /* .ratio = */ ratio
    );
    rhsKeyFrames_.emplace_back(
        /* .pos = */ rhs.pos.getXmf(),
        /* .padding = */ 0.f,
        /* .scale = */ rhs.scale.getXmf(),
        /* .padding2 = */ 0.f,
        /* .rot = */ rhs.rot.getXmf()
    );

    return lhsKeyFrames_.size() - 1u;
}

void AnimInterpolation::preCompute(D3D12GfxCmdList& cmdList) {
    cmdList.get()->SetPipelineState(protocol_.get().Get());
    resultMatrices_.clear();

    threadGroupCnt_ = (lhsKeyFrames_.size() + shader().groupSizeX - 1) / shader().groupSizeX;

    if (lhsKeyFrames_.size() > shader().maxKeyFrameCnt()) {
        throw GFX_EXCEPT("[Description]: MatMul::preCompute: too many matrices.");
    }


    shader().lhsKeyFrames_.stage(lhsKeyFrames_.data(), lhsKeyFrames_.size() * sizeof(sr::KeyFrame));
    shader().rhsKeyFrames_.stage(rhsKeyFrames_.data(), rhsKeyFrames_.size() * sizeof(sr::KeyFrame));
}

void AnimInterpolation::compute(D3D12GfxCmdList& cmdList) {
    cmdList.get()->Dispatch(static_cast<UINT>(threadGroupCnt_), 1u, 1u);
}

void AnimInterpolation::postCompute(D3D12GfxCmdList& cmdList) {
    shader().resultMatrices_.readback(cmdList, shader().resultMatricesSrc_);
}

void AnimInterpolation::postExecution() {
    resultMatrices_.reserve(threadGroupCnt_ * shader().groupSizeX);
    shader().resultMatrices_.mapReadbackResult();
    auto pMatrices = shader().resultMatrices_.getReadbackResult<dx::XMFLOAT4X4>();
    for (std::size_t i = 0u; i < threadGroupCnt_ * shader().groupSizeX; ++i) {
        resultMatrices_.emplace_back(dx::loadMat(pMatrices[i]));
        resultMatrices_.back() = mu::transpose(resultMatrices_.back());
    }

    shader().resultMatrices_.unmapReadbackResult();

    threadGroupCnt_ = 0u;
    lhsKeyFrames_.clear();
    rhsKeyFrames_.clear();
}

}   // namespace gfx::d3d12::cp

}   // namespace gfx::d3d12

}   // namespace gfx