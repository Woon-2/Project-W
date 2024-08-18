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

    auto repPos = pos().represent( parent );
    auto repUp = up.represent( parent );

    if (focused()) {
        auto repAt = focusPos_->represent( parent );

        view_ = mu::lookAt(repPos, repAt, repUp);

        // to keep the look direction when the camera loses focus
        lookDir_ = coord::Vec3( pParent, repAt - repPos );
    } else {
        auto repAt = repPos + lookDir_.represent( parent );
        view_ = mu::lookAt(repPos, repAt, repUp);
    }
}

}