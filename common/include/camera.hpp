#ifndef __CAMERA_HPP
#define __CAMERA_HPP

#include "gfx.hpp"
#include "coord.hpp"

#include <optional>

// Undefine near and far macros from minwindef.h
// It's annoying...
#ifdef near
#undef near
#endif

#ifdef far
#undef far
#endif
// ==============================================

namespace gfx {

class CameraScene : public IScene {
public:
    static constexpr std::size_t meshIdx = 0u;
    static constexpr std::size_t worldIdx = 1u;
    static constexpr std::size_t viewIdx = 2u;
    static constexpr std::size_t projIdx = 3u;

    CameraScene(const class Camera& camera);

protected:
    void fillViewProj(DrawInfo& drawInfo) const;

private:
    mu::Mat4x4 view_;
    mu::Mat4x4 proj_;
};

class Camera {
public:
    friend CameraScene;

    static constexpr auto defFov = 90.f;
    static constexpr auto defAspect = 16.f / 9.f;
    static constexpr auto defNear = 0.1f;
    static constexpr auto defFar = 1000.f;

    struct Config {
        float fov = defFov;
        float aspect = defAspect;
        float near = defNear;
        float far = defFar;
    };

    Camera(const coord::System& baseCoordSys)
        : Camera(baseCoordSys, Config()) {}
    
    Camera(const coord::System& baseCoordSys, const Config& config)
        : coordSys_(), config_(config), proj_(
            mu::persp(config_.fov, config_.aspect,config_.near, config_.far)
        ), focusPos_(), lookDir_(baseCoordSys) {
        coordSys_.setParent(&baseCoordSys);
    }

    const Config& config() const NOEXCEPT {
        return config_;
    }

    void updateConfig(const Config& config) {
        config_ = config;
        proj_ = mu::persp(config_.fov, config_.aspect,config_.near, config_.far);
    }

    coord::System& coordSys() NOEXCEPT {
        return coordSys_;
    }

    const coord::System& coordSys() const NOEXCEPT {
        return coordSys_;
    }

    bool focused() const NOEXCEPT {
        return focusPos_.has_value();
    }

    coord::Pt3 pos() const {
        return coord::Pt3(coordSys_, mu::Vec3(0.f, 0.f, 0.f));
    }

    void focus(const coord::Pt3& target) {
        focusPos_ = target;
        const auto& parent = *coordSys_.parent();
        lookDir_ = coord::Vec3( parent, target.represent( parent ) - pos().represent( parent ) );
    }

    void unfocus() {
        focusPos_.reset();
    }

    void lookTo(const coord::Vec3& lookDir) {
        if (focused()) {
            unfocus();
        }
        lookDir_ = lookDir;
    }

    void updateView();

private:
    const mu::Mat4x4 MU_CALLCONV view() const {
        return view_;
    }

    const mu::Mat4x4 MU_CALLCONV proj() const {
        return proj_;
    }

    coord::System coordSys_;
    Config config_;
    mu::Mat4x4 view_;
    mu::Mat4x4 proj_;
    std::optional<coord::Pt3> focusPos_;
    coord::Vec3 lookDir_;
};

}   // namespace gfx

#endif // __CAMERA_HPP