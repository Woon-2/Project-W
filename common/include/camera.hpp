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

/**
 * @brief A class representing a scene viewd by a camera.     
 * It's designed to be base class of the scene classes in the graphics API classes.     
 * It comes with utility functions to fill the Camera's view and projection matrices in the DrawInfo.
 * @details The CameraScene::fillViewProj function fills the view and projection matrices in the DrawInfo.     
 * The indices to get the matrices are defined as static constexpr members.    
 * - viewIdx: 2, the index of the view matrix in the DrawInfo.    
 * - projIdx: 3, the index of the projection matrix in the DrawInfo.
 * @see IScene Camera
 */
class CameraScene : public IScene {
public:
    static constexpr std::size_t meshIdx = 0u;  ///< The index of the mesh in the DrawInfo (as recommendation, it does not fills mesh).
    static constexpr std::size_t worldIdx = 1u; ///< The index of the world matrix in the DrawInfo (as recommendation, it does not fills world matrix).
    static constexpr std::size_t viewIdx = 2u; ///< The index of the view matrix in the DrawInfo.
    static constexpr std::size_t projIdx = 3u; ///< The index of the projection matrix in the DrawInfo.

    /**
     * @brief Setup the view and projection matrices with the camera.   
     * @note The matrices are value copied from the camera,    
     * so the camera's update after CameraScene construction does not affect CameraScene's behavior.
     * @param camera The camera to setup the view and projection matrices.
     * @see Camera fillViewProj
     */
    CameraScene(const class Camera& camera);

protected:
    /**
     * @brief Fills the view and projection matrices in the DrawInfo.
     * @param drawInfo The reference of the DrawInfo to fill the view and projection matrices.
     * @details It fills the view matrix in the CameraScene::viewIdx and the projection matrix in the CameraScene::projIdx of the DrawInfo.
     * @note It's recommended to call this function in the IScene::iteration function of the derived class.
     * @see Camera IScene::iteration
     */
    void fillViewProj(DrawInfo& drawInfo) const;

private:
    mu::Mat4x4 view_;
    mu::Mat4x4 proj_;
};

/**
 * @brief A class representing a camera in 3D space independent of the graphics API.     
 * Focusing on a target, looking to a direction, and updating the coords are supported.    
 * @note The camera must have parent coordinate system, in most cases a coordinate system representing the world space.
 * @details Camera's coord::System member determines what camera views, affecting the view matrix.    
 * Camera's Camera::Config member determines how the objects are projected to the camera, affecting the projection matrix.    
 * 
 * @note To batch process the modification of camera view, the view matrix is not updated automatically.    
 * Instead, Camera::updateView function is provided to update the view matrix.    
 * 
 * The view and projection matrices is not directly accessible.     
 * To build scene with camera which means the scene's DrawInfo s is filled with the view and projection matrices,    
 * Use of CameraScene implementation is required.
 * @see CameraScene coord::System
 */
class Camera {
public:
    friend CameraScene;

    static constexpr auto defFov = mu::Degree(90.f);
    static constexpr auto defAspect = 16.f / 9.f;
    static constexpr auto defNear = 0.1f;
    static constexpr auto defFar = 1000.f;

    /**
     * @brief A struct representing the configuration of the camera.
     * @details The configuration includes the field of view, the aspect ratio, the near plane, and the far plane.    
     * The default values are defined as Camera::defFov, Camera::defAspect, Camera::defNear, and Camera::defFar.     
     * It affects the projection matrix of the camera.
     */
    struct Config {
        mu::Degree fov = defFov;    ///< The Y field of view of the camera.
        float aspect = defAspect;   ///< The aspect ratio(width / height) of the camera.
        float near = defNear;       ///< The near plane of the camera.
        float far = defFar;         ///< The far plane of the camera.
    };

    /**
     * @brief Constructs a camera with the default configuration
     */
    Camera()
        : Camera(coord::System(), Config()) {}
    /**
     * @brief Constructs a camera with the default configuration,    
     * and initializes the camera's coordinate system by copying coord::System from the base coordinate system.
     * @param baseCoordSys The base coordinate system to initialize the camera's coordinate system.
     * @see coord::System
     */
    Camera(const coord::System& baseCoordSys)
        : Camera(baseCoordSys, Config()) {}
    /**
     * @brief Constructs a camera with the specified configuration.
     * @param config The configuration of the camera.
     * @see Camera::Config
     */
    Camera(const Config& config)
        : Camera(coord::System(), config) {}
    /**
     * @brief Constructs a camera with the specified configuration,
     * and initializes the camera's coordinate system by copying coord::System from the base coordinate system.
     * @param baseCoordSys The base coordinate system to initialize the camera's coordinate system.
     * @param config The configuration of the camera.
     * @see coord::System Camera::Config
     */
    Camera(const coord::System& baseCoordSys, const Config& config)
        : coordSys_(baseCoordSys), config_(config), proj_(
            mu::persp(config_.fov, config_.aspect,config_.near, config_.far)
        ), focusPos_(), lookDir_(&baseCoordSys) {}

    /**
     * @brief Gets the configuration of the camera.
     * @return const Config& The reference of the configuration of the camera.
     * @see Camera::Config
     */
    const Config& config() const NOEXCEPT {
        return config_;
    }
    /**
     * @brief Updates the configuration of the camera.
     * @param config The new configuration of the camera.
     * @see Camera::Config
     */
    void updateConfig(const Config& config) {
        config_ = config;
        proj_ = mu::persp(config_.fov, config_.aspect,config_.near, config_.far);
    }
    /**
     * @brief Gets the coordinate system of the camera.
     * @return coord::System& The reference of the coordinate system of the camera.
     * @see coord::System
     */
    coord::System& coordSys() NOEXCEPT {
        return coordSys_;
    }
    /**
     * @brief Gets the coordinate system of the camera.
     * @return const coord::System& The reference of the coordinate system of the camera.
     * @see coord::System
     */
    const coord::System& coordSys() const NOEXCEPT {
        return coordSys_;
    }

    coord::Pt3 pos() const {
        return coord::Pt3(&coordSys_, mu::Vec3(0.f, 0.f, 0.f));
    }

    /**
     * @brief Checks if the camera is focused on a target.    
     * @note If Camera::lookTo or Camera::unfocous is called, the camera loses focus on the target previously had.
     * @return bool True if the camera is focused on a target, false otherwise.
     * @see focus unfocus lookTo
     */
    bool focused() const NOEXCEPT {
        return focusPos_.has_value();
    }
    /**
     * @brief Focuses the camera on a target.
     * @note If the camera refocus on a different target or Camera::lookTo or Camera::unfocous is called,     
     * the camera loses focus on the target previously had.
     * @note When camera loses focus, the camera keeps the look direction when it was focused.
     * @param target The target to focus on.
     * @see focused unfocus lookTo
     */
    void focus(const coord::Pt3& target) {
        focusPos_ = target;
        auto parent = coordSys_.parent();

        if (!parent) {
            throw GFX_EXCEPT("Camera must have a parent coordinate system.");
        }

        lookDir_ = coord::Vec3( parent, target.represent( *parent ) - pos().represent( *parent ) );
    }
    /**
     * @brief Unfocus the camera.
     * @note When camera loses focus, the camera keeps the look direction when it was focused.
     * @see focused focus lookTo
     */
    void unfocus() {
        focusPos_.reset();
    }
    /**
     * @brief Looks to a direction.
     * @note If the camera has been focused on a target, the camera loses focus on the target.
     * @param lookDir The direction to look to.
     * @see focused focus unfocus
     */
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