#ifndef __d3d12RenderPass_HPP
#define __d3d12RenderPass_HPP

#include "d3d12util/d3d12ShaderXX.hpp"
#include "d3d12util/d3d12Low.hpp"
#include "d3d12util/d3d12ResourceXX.hpp"
#include "coord.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <map>

namespace gfx {

namespace d3d12 {

class Camera {
public:
    static constexpr auto defFov = mu::Degree(90.f);
    static constexpr auto defAspect = 16.f / 9.f;
    static constexpr auto defNear = 0.1f;
    static constexpr auto defFar = 1000.f;

    enum class FocusMode {
        None, LookAt, LookTo
    };

    struct Config {
        mu::Degree fov = defFov;
        float aspect = defAspect;
        float nearZ = defNear;
        float farZ = defFar;
    };

    Camera(coord::System& baseCoordSys)
        : Camera(baseCoordSys, Config()) {}

    Camera(coord::System& baseCoordSys, const Config& config)
        : coordSys_(), config_(config), view_(), proj_(
            mu::persp(config_.fov, config_.aspect,config_.nearZ, config_.farZ)
        ), repPos_(), repUp_() {
        coordSys_.setParent(&baseCoordSys);
    }

    const Config& config() const NOEXCEPT {
        return config_;
    }

    void updateConfig(const Config& config) {
        config_ = config;
        proj_ = mu::persp(config_.fov, config_.aspect,config_.nearZ, config_.farZ);
    }

    void updateView();

    void MU_CALLCONV focusAt(mu::Vec3 focus) NOEXCEPT {
        focus_ = focus;
        focusMode_ = FocusMode::LookAt;
    }

    void MU_CALLCONV focusTo(mu::Vec3 focus) NOEXCEPT {
        focus_ = focus;
        focusMode_ = FocusMode::LookTo;
    }

    void defocus() NOEXCEPT {
        focusMode_ = FocusMode::None;
    }

    coord::System& coord() NOEXCEPT {
        return coordSys_;
    }

    const coord::System& coord() const NOEXCEPT {
        return coordSys_;
    }

    mu::Mat4x4 MU_CALLCONV view() const NOEXCEPT {
        return view_;
    }

    mu::Mat4x4 MU_CALLCONV proj() const NOEXCEPT {
        return proj_;
    }

    mu::Vec3 MU_CALLCONV repPos() const NOEXCEPT {
        return repPos_;
    }

    mu::Vec3 MU_CALLCONV repUp() const NOEXCEPT {
        return repUp_;
    }

    mu::Vec3 MU_CALLCONV repFwd() const NOEXCEPT {
        return repFwd_;
    }

private:
    coord::System coordSys_;
    Config config_;
    mu::Mat4x4 view_;
    mu::Mat4x4 proj_;
    mu::Vec3 repPos_;
    mu::Vec3 repUp_;
    mu::Vec3 repFwd_;
    mu::Vec3 focus_;
    FocusMode focusMode_;
};

class RenderPass {
public:
    RenderPass() = default;
    RenderPass(std::string_view id) : renderPassID_(id) {}
    RenderPass(std::string&& id) : renderPassID_(std::move(id)) {}
    virtual ~RenderPass() = default;

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

namespace rp {

class PBRIllumination : public gfx::d3d12::RenderPass {
public:
    PBRIllumination(D3D12Device& device, ShaderPBRIllumination& shader)
        : protocol_( shader.makeProtocol( device,
            RenderProtocol::Desc{ makeDesc() }
        ) ), lights_(), batch_() {}

    void setViewport(const D3D12_VIEWPORT& vp);

    const D3D12_VIEWPORT& viewport() const NOEXCEPT {
        return viewport_;
    }

    void preRender(D3D12GfxCmdList& cmdList) override;
    void render(D3D12GfxCmdList& cmdList) override;
    void postRender(D3D12GfxCmdList& cmdList) override;

    void trackModel(Model* pModel);

private:
    ShaderPBRIllumination& shader() noexcept {
        return static_cast<ShaderPBRIllumination&>(protocol_.shader());
    }
    const ShaderPBRIllumination& shader() const noexcept {
        return static_cast<const ShaderPBRIllumination&>(protocol_.shader());
    }

    static RenderProtocol::Desc makeDesc();

    D3D12_VIEWPORT viewport_;
    RenderProtocol protocol_;
    std::vector<const sr::Light*> lights_;
    std::vector< std::pair<Mesh*, mu::Mat4x4> > batch_;
    const Camera* pCamera_;
};

}   // namespace gfx::d3d12::rp

}   // namespace gfx::d3d12

}   // namespace gfx

#endif  // __d3d12RenderPass_HPP