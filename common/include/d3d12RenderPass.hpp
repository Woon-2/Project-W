#ifndef __d3d12RenderPass_HPP
#define __d3d12RenderPass_HPP

#include "d3d12Low.hpp"

#include <string>

namespace gfx {

namespace d3d12 {

class RenderPass {
public:
    virtual void preRender(D3D12GfxCmdList& cmdList) = 0;
    virtual void render(D3D12GfxCmdList& cmdList) = 0;
    virtual void postRender(D3D12GfxCmdList& cmdList) = 0;

    void setRenderPassID(const std::string& renderPassID) {
        renderPassID_ = renderPassID;
    }
    const auto& renderPassID() const noexcept {
        return renderPassID_;
    }

private:
    std::string renderPassID_;
};

}   // namespace gfx::d3d12

}   // namespace gfx

#endif  // __d3d12RenderPass_HPP