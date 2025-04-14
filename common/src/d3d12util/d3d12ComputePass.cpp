#include "d3d12util/d3d12ComputePass.hpp"

#include <ranges>
#include <algorithm>

namespace gfx {

namespace d3d12 {

namespace cp {

MatMul::MatMul(D3D12Device& device, ShaderMatMul& shader)
    : ComputePass(id), protocol_(shader.makeProtocol(device, makeDesc())),
      lhsMatrices_(), rhsMatrices_(), resultMatrices_(), threadGroupCnt_(0u) {
    checkRequiredTextures();
}

ComputeProtocol::Desc MatMul::makeDesc() {
    return ComputeProtocol::Desc{
        .nodeMask = 0u
    };
}

void MatMul::preCompute(D3D12GfxCmdList& cmdList) {
    cmdList.get()->SetPipelineState(protocol_.get().Get());
    resultMatrices_.clear();

    threadGroupCnt_ = (lhsMatrices_.size() + shader().groupSizeX - 1) / shader().groupSizeX;

    auto srLhsMatrices = std::vector<dx::XMFLOAT4X4>(
        threadGroupCnt_ * shader().groupSizeX
    );
    auto srRhsMatrices = std::vector<dx::XMFLOAT4X4>(srLhsMatrices.size());

    if (srLhsMatrices.size() > shader().maxMatrixCnt()) {
        throw GFX_EXCEPT("[Description]: MatMul::preCompute: too many matrices.");
    }

    // as gpus are column-major, we need to transpose the matrices.
    std::ranges::transform(lhsMatrices_, srLhsMatrices.begin(), [](const auto& mat) {
        return dx::XMFLOAT4X4{mu::transpose(mat).getXmf()};
    });
    std::ranges::transform(rhsMatrices_, srRhsMatrices.begin(), [](const auto& mat) {
        return dx::XMFLOAT4X4{mu::transpose(mat).getXmf()};
    });

    shader().lhsMatrices_.stage(srLhsMatrices.data(), srLhsMatrices.size() * sizeof(dx::XMFLOAT4X4));
    shader().rhsMatrices_.stage(srRhsMatrices.data(), srRhsMatrices.size() * sizeof(dx::XMFLOAT4X4));
}

void MatMul::compute(D3D12GfxCmdList& cmdList) {
    cmdList.get()->Dispatch(static_cast<UINT>(threadGroupCnt_), 1u, 1u);
}

void MatMul::postCompute(D3D12GfxCmdList& cmdList) {
    shader().resultMatrices_.readback(cmdList, shader().resultMatricesSrc_);
    
    resultMatrices_.reserve(threadGroupCnt_ * shader().groupSizeX);
    auto pMatrices = shader().resultMatrices_.getReadbackResult<dx::XMFLOAT4X4>();
    for (std::size_t i = 0u; i < threadGroupCnt_ * shader().groupSizeX; ++i) {
        resultMatrices_.emplace_back(dx::loadMat(pMatrices[i]));
    }

    shader().resultMatrices_.completeReadback();

    threadGroupCnt_ = 0u;
    lhsMatrices_.clear();
    rhsMatrices_.clear();
}

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
        /* .rot = */ lhs.rot.getXmf(),
        /* .scale = */ lhs.scale.getXmf(),
        /* .ratio = */ ratio
    );
    rhsKeyFrames_.emplace_back(
        /* .pos = */ rhs.pos.getXmf(),
        /* .rot = */ rhs.rot.getXmf(),
        /* .scale = */ rhs.scale.getXmf()
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

    resultMatrices_.reserve(threadGroupCnt_ * shader().groupSizeX);
    auto pMatrices = shader().resultMatrices_.getReadbackResult<dx::XMFLOAT4X4>();
    for (std::size_t i = 0u; i < threadGroupCnt_ * shader().groupSizeX; ++i) {
        resultMatrices_.emplace_back(dx::loadMat(pMatrices[i]));
    }

    shader().resultMatrices_.completeReadback();

    threadGroupCnt_ = 0u;
    lhsKeyFrames_.clear();
    rhsKeyFrames_.clear();
}

}   // namespace gfx::d3d12::cp

}   // namespace gfx::d3d12

}   // namespace gfx