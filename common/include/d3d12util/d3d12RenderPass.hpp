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
#include <type_traits>

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

    Camera()
        : Camera(Config()) {}

    Camera(const Config& config)
        : coordMovement_(), coordRotation_(), config_(config), view_(), proj_(
            mu::persp(config_.fov, config_.aspect,config_.nearZ, config_.farZ)
        ), repPos_(), repUp_(), repFwd_(), focus_(), focusMode_(FocusMode::None) {
        coordRotation_.setParent(&coordMovement_);
    }

    const Config& config() const NOEXCEPT {
        return config_;
    }

    void updateConfig(const Config& config) {
        config_ = config;
        proj_ = mu::persp(config_.fov, config_.aspect,config_.nearZ, config_.farZ);
    }

    void updateView();

    void MU_CALLCONV focusAt(mu::Vec3 focus, mu::Vec3 up) NOEXCEPT {
        focus_ = focus;
        focusMode_ = FocusMode::LookAt;

        repUp_ = up;
    }

    void MU_CALLCONV focusTo(mu::Vec3 focus, mu::Vec3 up) NOEXCEPT {
        focus_ = focus;
        focusMode_ = FocusMode::LookTo;

        repUp_ = up;
    }

    void defocus() NOEXCEPT {
        focusMode_ = FocusMode::None;
    }

    coord::System& coordMovement() NOEXCEPT {
        return coordMovement_;
    }

    const coord::System& coordMovement() const NOEXCEPT {
        return coordMovement_;
    }

    coord::System& coordRotation() NOEXCEPT {
        return coordRotation_;
    }

    const coord::System& coordRotation() const NOEXCEPT {
        return coordRotation_;
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
    coord::System coordMovement_;
    coord::System coordRotation_;
    Config config_;
    mu::Mat4x4 view_;
    mu::Mat4x4 proj_;
    mu::Vec3 repPos_;
    mu::Vec3 repUp_;
    mu::Vec3 repFwd_;
    mu::Vec3 focus_;
    FocusMode focusMode_;
};

// Render targets' low level resources should reside in shaders or window,
// and below render target classes should not own them.
// By doing so, we can avoid the need to manage the resources' lifetimes
// in render target classes.
class IRenderTarget {
public:
    friend class RenderTargets;
    virtual ~IRenderTarget() = default;

private:
    virtual void onPush(D3D12GfxCmdList& cmdList) = 0;
    virtual void onPop(D3D12GfxCmdList& cmdList) = 0;
    virtual void onBind(D3D12GfxCmdList& cmdList) = 0;
    virtual void onClear(D3D12GfxCmdList& cmdList) = 0;
};

class RenderTargets {
public:
    // should update sSpecifierStrings as well if Specifiers are updated
    enum class Specifier {
        Main,
        Shadow,
        SIZE
    };

    void pushTarget(D3D12GfxCmdList& cmdList, Specifier spec, IRenderTarget* pTarget);
    IRenderTarget* popTarget(D3D12GfxCmdList& cmdList, Specifier spec);
    template <class ... Specs>
        requires (std::is_same_v<Specifier, std::remove_cvref_t<Specs>> && ...)
    void bind(D3D12GfxCmdList& cmdList, Specs ... specs) {
        (map_.at(specs)->onBind(cmdList), ...);
    }
    template <class ... Specs>
        requires (std::is_same_v<Specifier, std::remove_cvref_t<Specs>> && ...)
    void clear(D3D12GfxCmdList& cmdList, Specs ... specs) {
        (map_.at(specs)->onClear(cmdList), ...);
    }

private:
    static std::string sSpecifierStrings[etoi(Specifier::SIZE)];

    std::map<Specifier, IRenderTarget*> map_;
};

template <class TWindow>
class MainRenderTarget : public IRenderTarget {
public:
    MainRenderTarget(TWindow& window)
        : window_(window) {}

private:
    void onPush(D3D12GfxCmdList& cmdList) override {}

    void onBind(D3D12GfxCmdList& cmdList) override {
        window_.setRenderTarget(cmdList);
    }

    void onPop(D3D12GfxCmdList& cmdList) override {
        window_.setPresent(cmdList);
    }

    void onClear(D3D12GfxCmdList& cmdList) override {
        window_.clearRenderTarget(cmdList);
        window_.clearDepthStencil(cmdList);
    }

    TWindow& window_;
};

class ShadowMaterial : public IRenderTarget, public Material {
public:
    ShadowMaterial( Texture& mapResource, const DescriptorCPU& dsv,
        const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
        const DescriptorGPU& srv, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
    );

private:
    void onPush(D3D12GfxCmdList& cmdList) override;
    void onBind(D3D12GfxCmdList& cmdList) override;
    void onPop(D3D12GfxCmdList& cmdList) override;
    void onClear(D3D12GfxCmdList& cmdList) override;

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc_;
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc_;
    const DescriptorGPU& srv_;
    const DescriptorCPU& dsv_;
    Texture& mapResource_;
};

struct WorldLight {
    using Type = sr::Light::Type;

    struct Config {
        union {
            struct Ortho {
                float width;
                float height;
                float nearZ;
                float farZ;
            } ortho;

            struct NonDirectional {
                float fovy;
                float aspect;
                float nearZ;
                float farZ;
            } perspective;
        };
    };

    sr::Light toViewLight(const Camera& camera) const;
    // for directional lights
    mu::Mat4x4 MU_CALLCONV view(const Camera& camera) const;
    // for point lights
    // std::array<mu::Mat4x4, 6> MU_CALLCONV views() const;
    mu::Mat4x4 MU_CALLCONV proj() const;
    mu::Mat4x4 MU_CALLCONV viewProj(const Camera& camera) const;

    Config config;
    mu::Vec3 color;
    // absolute position for point lights,
    mu::Vec3 pos;
    mu::Vec3 dir;
    mu::Vec3 atten;
    float falloff;
    float cosTheta;
    float cosPhi;
    float intensity;
    float distanceToCamera;
    Type type;
};

class RenderPass {
public:
    using VBLayoutIdx = std::size_t;

    RenderPass() = default;
    RenderPass(const char* id) : RenderPass(std::string_view(id)) {}
    RenderPass(std::string_view id) : renderPassID_(id) {}
    RenderPass(std::string&& id) : renderPassID_(std::move(id)) {}
    virtual ~RenderPass() = default;

    virtual void preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) = 0;
    virtual void render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) = 0;
    virtual void postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) = 0;

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
    static constexpr const char* id = "PBRIllumination";

    PBRIllumination( D3D12Device& device, ShaderPBRIllumination& shader,
        const D3D12_VIEWPORT& vp = D3D12_VIEWPORT{}
    );

    void setViewport(const D3D12_VIEWPORT& vp);

    const D3D12_VIEWPORT& viewport() const NOEXCEPT {
        return viewport_;
    }

    void preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
    void render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
    void postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;

    void trackModel(Model* pModel);
    void setCamera(const Camera* pCamera) NOEXCEPT {
        pCamera_ = pCamera;
    }
    void addLight(const WorldLight* pLight) NOEXCEPT {
        lights_.push_back(pLight);
    }

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
    std::vector<const WorldLight*> lights_;
    std::vector< std::tuple<Submesh*, VBLayoutIdx, mu::Mat4x4> > batch_;
    const Camera* pCamera_;
    ShadowMaterial* pShadowMaterial_;
};

class PBRIlluminationTerrain : public gfx::d3d12::RenderPass {
public:
    static constexpr const char* id = "PBRIlluminationTerrain";

    PBRIlluminationTerrain( D3D12Device& device, ShaderPBRIlluminationTerrain& shader,
        const D3D12_VIEWPORT& vp = D3D12_VIEWPORT{}
    );

    void setViewport(const D3D12_VIEWPORT& vp);

    const D3D12_VIEWPORT& viewport() const NOEXCEPT {
        return viewport_;
    }

    void preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
    void render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
    void postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;

    void trackModel(Model* pModel);
    void setCamera(const Camera* pCamera) NOEXCEPT {
        pCamera_ = pCamera;
    }
    void addLight(const WorldLight* pLight) NOEXCEPT {
        lights_.push_back(pLight);
    }

private:
    ShaderPBRIlluminationTerrain& shader() noexcept {
        return static_cast<ShaderPBRIlluminationTerrain&>(protocol_.shader());
    }
    const ShaderPBRIlluminationTerrain& shader() const noexcept {
        return static_cast<const ShaderPBRIlluminationTerrain&>(protocol_.shader());
    }

    static RenderProtocol::Desc makeDesc();

    D3D12_VIEWPORT viewport_;
    RenderProtocol protocol_;
    std::vector<const WorldLight*> lights_;
    std::vector< std::tuple<Submesh*, VBLayoutIdx, mu::Mat4x4> > batch_;
    const Camera* pCamera_;
};

class ShadowMap : public gfx::d3d12::RenderPass {
public:
    static constexpr const char* id = "ShadowMap";

    ShadowMap( D3D12Device& device, ShaderShadowMap& shader,
        DescriptorRange<DescriptorHeapCPU>& dsvRange,
        const D3D12_VIEWPORT& vp = D3D12_VIEWPORT{}
    );

    void setViewport(const D3D12_VIEWPORT& vp);

    const D3D12_VIEWPORT& viewport() const NOEXCEPT {
        return viewport_;
    }

    void preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
    void render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
    void postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;

    void trackModel(Model* pModel);
    void setCamera(const Camera* pCamera) NOEXCEPT {
        pCamera_ = pCamera;
    }
    void setLight(const WorldLight* pLight);

private:
    ShaderShadowMap& shader() noexcept {
        return static_cast<ShaderShadowMap&>(protocol_.shader());
    }
    const ShaderShadowMap& shader() const noexcept {
        return static_cast<const ShaderShadowMap&>(protocol_.shader());
    }

    static RenderProtocol::Desc makeDesc();

    D3D12_SHADER_RESOURCE_VIEW_DESC shadowMapSrvDesc_;
    D3D12_DEPTH_STENCIL_VIEW_DESC shadowMapDsvDesc_;
    std::size_t idxShadowMapDsv_;
    ShadowMaterial shadowMaterial_;
    D3D12_VIEWPORT viewport_;
    RenderProtocol protocol_;
    const WorldLight* pLight_;
    std::vector< std::tuple<Submesh*, VBLayoutIdx, mu::Mat4x4> > batch_;
    const Camera* pCamera_;
};

class ScreenQuad : public gfx::d3d12::RenderPass {
public:
    static constexpr const char* id = "ScreenQuad";

    ScreenQuad( D3D12Device& device, ShaderScreenQuad& shader,
        const D3D12_VIEWPORT& vp = D3D12_VIEWPORT{}
    );

    void setViewport(const D3D12_VIEWPORT& vp);

    const D3D12_VIEWPORT& viewport() const NOEXCEPT {
        return viewport_;
    }

    void preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
    void render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
    void postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;

private:
    ShaderScreenQuad& shader() noexcept {
        return static_cast<ShaderScreenQuad&>(protocol_.shader());
    }
    const ShaderScreenQuad& shader() const noexcept {
        return static_cast<const ShaderScreenQuad&>(protocol_.shader());
    }

    static RenderProtocol::Desc makeDesc();

    D3D12_VIEWPORT viewport_;
    RenderProtocol protocol_;
};

class Tessellation : public gfx::d3d12::RenderPass {
public:
    static constexpr const char* id = "Tessellation";

    Tessellation( D3D12Device& device, ShaderTessellation& shader,
        const D3D12_VIEWPORT& vp = D3D12_VIEWPORT{}
    );

    void setViewport(const D3D12_VIEWPORT& vp);

    const D3D12_VIEWPORT& viewport() const NOEXCEPT {
        return viewport_;
    }    

    void preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
    void render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
    void postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
    
    void trackChunk(const LevelChunkModel* pChunk);
    void setCamera(const Camera* pCamera) NOEXCEPT {
        pCamera_ = pCamera;
    }
    void addLight(const WorldLight* pLight) NOEXCEPT {
        lights_.push_back(pLight);
    }

private:
    ShaderTessellation& shader() noexcept {
        return static_cast<ShaderTessellation&>(protocol_.shader());
    }
    const ShaderTessellation& shader() const noexcept {
        return static_cast<const ShaderTessellation&>(protocol_.shader());
    }

    static RenderProtocol::Desc makeDesc();

    D3D12_VIEWPORT viewport_;
    RenderProtocol protocol_;
    std::vector<const WorldLight*> lights_;
    std::vector<const LevelChunkModel*> batch_;
    const Camera* pCamera_;
};

}   // namespace gfx::d3d12::rp

}   // namespace gfx::d3d12

}   // namespace gfx

#endif  // __d3d12RenderPass_HPP