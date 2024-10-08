#ifndef __D3d12SpecialRendertargets_HPP
#define __D3d12SpecialRendertargets_HPP

#include "d3d12core.hpp"
#include "d3d12texture.hpp"

namespace gfx {

namespace d3d12 {

class ShadowTarget : public IRenderTarget {
public:
    bool castableTo(RenderTargetType type) const override;
    std::any cast(RenderTargetType type) override;
    void preRender(IRenderContext& renderContext) override;
    void postRender(IRenderContext& renderContext) override;
    void clear(IRenderContext& renderContext) override;

private:
    Texture* pShadowTex_;
};

}   // namespace gfx::d3d12

}   // namespace gfx

#endif  // __D3d12SpecialRendertargets_HPP