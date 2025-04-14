#ifndef __d3d12ComputePass_HPP
#define __d3d12ComputePass_HPP

#include "d3d12util/d3d12ShaderXX.hpp"
#include "d3d12util/d3d12Low.hpp"
#include "d3d12util/d3d12ResourceXX.hpp"

#ifndef DXMATH_VEC_UTIL
#define DXMATH_VEC_UTIL
#endif
#ifndef DXMATH_MAT_UTIL
#define DXMATH_MAT_UTIL
#endif
#ifndef DXMATH_QUAT_UTIL
#define DXMATH_QUAT_UTIL
#endif
#include "mathUtil.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gfx {

namespace d3d12 {

enum class ComputePassTextures {};

class ComputePass {
public:
    using ResKey = std::string;

    ComputePass() = default;
    ComputePass(const char* id) : ComputePass(std::string_view(id)) {}
    ComputePass(std::string_view id) : computePassID_(id) {}
    ComputePass(std::string&& id) : computePassID_(std::move(id)) {}
    virtual ~ComputePass() = default;

    virtual void preCompute(D3D12GfxCmdList& cmdList) = 0;
    virtual void compute(D3D12GfxCmdList& cmdList) = 0;
    virtual void postCompute(D3D12GfxCmdList& cmdList) = 0;

    void setComputePassID(const std::string& computePassID) {
        computePassID_ = computePassID;
    }

    const auto& computePassID() const noexcept {
        return computePassID_;
    }

    void mapTexture(ComputePassTextures type, Texture* pTexture) {
        textureMap_[type] = pTexture;
    }

    Texture* getTexture(ComputePassTextures type) const {
        auto it = textureMap_.find(type);
        if (it != textureMap_.end()) {
            return it->second;
        }
        return nullptr;
    }

    virtual std::vector<ComputePassTextures> requiredTextures() const {
        return {};
    }

protected:
    void checkRequiredTextures() const {
        for (const auto& texID : requiredTextures()) {
            if (!textureMap_.contains(texID)) {
                throw std::runtime_error("Required texture not found: " + computePassID_);
            }
        }
    }

private:
    std::unordered_map<ComputePassTextures, Texture*> textureMap_;
    std::string computePassID_;
};

namespace cp {

class MatMul : public gfx::d3d12::ComputePass {
public:
    static constexpr const char* id = "MatMul";

    MatMul(D3D12Device& device, ShaderMatMul& shader);

    // matrix pairs are cleared after call of postCompute()
    std::size_t addMatrixPair(const mu::Mat4x4& lhs, const mu::Mat4x4& rhs) {
        lhsMatrices_.push_back(lhs);
        rhsMatrices_.push_back(rhs);
        return lhsMatrices_.size() - 1u;
    }

    void preCompute(D3D12GfxCmdList& cmdList) override;
    void compute(D3D12GfxCmdList& cmdList) override;
    void postCompute(D3D12GfxCmdList& cmdList) override;

    std::vector<mu::Mat4x4>& resultMatrices() {
        return resultMatrices_;
    }
    const std::vector<mu::Mat4x4>& resultMatrices() const {
        return resultMatrices_;
    }

private:
    ShaderMatMul& shader() noexcept {
        return static_cast<ShaderMatMul&>(protocol_.shader());
    }
    const ShaderMatMul& shader() const noexcept {
        return static_cast<const ShaderMatMul&>(protocol_.shader());
    }

    static ComputeProtocol::Desc makeDesc();

    ComputeProtocol protocol_;
    std::vector<mu::Mat4x4> lhsMatrices_;
    std::vector<mu::Mat4x4> rhsMatrices_;
    std::vector<mu::Mat4x4> resultMatrices_;
    std::size_t threadGroupCnt_;
};

class AnimInterpolation : public gfx::d3d12::ComputePass {
public:
    static constexpr const char* id = "AnimInterpolation";

    AnimInterpolation(D3D12Device& device, ShaderAnimInterpolation& shader);

    std::size_t addKeyFramePair(const KeyFrame& lhs, const KeyFrame& rhs, float ratio);

    void preCompute(D3D12GfxCmdList& cmdList) override;
    void compute(D3D12GfxCmdList& cmdList) override;
    void postCompute(D3D12GfxCmdList& cmdList) override;

    std::vector<mu::Mat4x4>& resultMatrices() {
        return resultMatrices_;
    }
    const std::vector<mu::Mat4x4>& resultMatrices() const {
        return resultMatrices_;
    }

private:
    ShaderAnimInterpolation& shader() noexcept {
        return static_cast<ShaderAnimInterpolation&>(protocol_.shader());
    }
    const ShaderAnimInterpolation& shader() const noexcept {
        return static_cast<const ShaderAnimInterpolation&>(protocol_.shader());
    }

    static ComputeProtocol::Desc makeDesc();

    ComputeProtocol protocol_;
    std::vector<sr::KeyFrame> lhsKeyFrames_;
    std::vector<sr::KeyFrame> rhsKeyFrames_;
    std::vector<mu::Mat4x4> resultMatrices_;
    std::size_t threadGroupCnt_;
};

}   // namespace gfx::d3d12::cp

}   // namespace gfx::d3d12

}   // namespace gfx
    

#endif  // __d3d12ComputePass_HPP