#include "camera.hpp"

namespace gfx {

CameraScene::CameraScene(const Camera& camera)
    : view_(camera.view()), proj_(camera.proj()) {}

void CameraScene::fillViewProj(DrawInfo& drawInfo) const {
    drawInfo.set(viewIdx, &view_);
    drawInfo.set(projIdx, &proj_);
}

void Camera::updateView() {
    auto pParent = coordSys_.parent();

    if (!pParent) {
        throw GFX_EXCEPT("Camera must have a parent coordinate system.");
    }

    auto up = coord::Vec3( &coordSys_, mu::Vec3(0.f, 1.f, 0.f) );
    const auto& parent = *pParent;

    if (focused()) {
        view_ = mu::lookAt(
            pos().represent( parent ),
            focusPos_->represent( parent ),
            up.represent( parent )
        );
    } else {
        view_ = mu::lookAt(
            pos().represent( parent ),
            pos().represent( parent ) + lookDir_.represent( parent ),
            up.represent( parent )
        );
    }
}

}