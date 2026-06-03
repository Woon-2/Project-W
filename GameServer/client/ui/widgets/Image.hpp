#ifndef __UI_IMAGE_HPP
#define __UI_IMAGE_HPP

#include "../UIElement.hpp"

namespace UI {

class Image : public UIElement {
public:
    const Texture* texture = nullptr;
    // uv' = uv * uvScaleBias.xy + uvScaleBias.zw (identity = full texture).
    // 아틀라스 텍스처의 셀 sub-rect를 샘플할 때 사용(예: 포트레이트 RT 슬롯 셀).
    XMFLOAT4 uvScaleBias = { 1.f, 1.f, 0.f, 0.f };
    // per-draw color multiplier (알파로 투명도 조절 가능).
    XMFLOAT4 colorMul = { 1.f, 1.f, 1.f, 1.f };

    void onRender(const RenderContext& rc) override;
};

}   // namespace UI

#endif  // __UI_IMAGE_HPP
