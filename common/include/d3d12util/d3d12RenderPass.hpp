#ifndef __d3d12RenderPass_HPP
#define __d3d12RenderPass_HPP

#include "stdafx.hpp"

#include "d3d12util/d3d12ShaderXX.hpp"
#include "d3d12util/d3d12Low.hpp"
#include "d3d12util/d3d12ResourceXX.hpp"
#include "coord.hpp"
#include "game/physicsSystem.hpp"

class AnimController;

namespace gfx {

namespace d3d12 {

// TODO: support orthogonal projection mode
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
        : coordMovement_(), coordRotation_(), config_(config), bv_(), view_(), proj_(
            mu::persp(config_.fov, config_.aspect,config_.nearZ, config_.farZ)
        ), repPos_(), repUp_(), repFwd_(), focus_(), focusMode_(FocusMode::None) {
        bv_.addCollider( BoundingFrustum{
            .origin = mu::Vec3(0.f, 0.f, 0.f),
            .orientation = mu::NQuat(),
            .fovy = config_.fov,
            .aspect = config_.aspect,
            .nearZ = config_.nearZ,
            .farZ = config_.farZ
        } );
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

    const BoundingVolumeNode& bvNode() const NOEXCEPT {
        return bv_;
    }

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
    BoundingVolumeNode bv_;
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
    ShadowMaterial();
    ShadowMaterial( Texture* pMapResource, const DescriptorCPU* pDsv,
        const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
        const DescriptorGPU* pSrv, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
    );

    const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc() const NOEXCEPT {
        return dsvDesc_;
    }

    const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc() const NOEXCEPT {
        return srvDesc_;
    }

    const DescriptorGPU& srv() const NOEXCEPT {
        return *pSrv_;
    }

    const DescriptorCPU& dsv() const NOEXCEPT {
        return *pDsv_;
    }

    Texture& texture() NOEXCEPT {
        return *pMapResource_;
    }

    const Texture& texture() const NOEXCEPT {
        return *pMapResource_;
    }

    bool valid() const NOEXCEPT {
        return pMapResource_ != nullptr && pSrv_ != nullptr && pDsv_ != nullptr;
    }

private:
    void onPush(D3D12GfxCmdList& cmdList) override;
    void onBind(D3D12GfxCmdList& cmdList) override;
    void onPop(D3D12GfxCmdList& cmdList) override;
    void onClear(D3D12GfxCmdList& cmdList) override;

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc_;
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc_;
    const DescriptorGPU* pSrv_;
    const DescriptorCPU* pDsv_;
    Texture* pMapResource_;
};

class ShadowArrayMaterial : public IRenderTarget, public Material {
public:
    ShadowArrayMaterial();
    ShadowArrayMaterial(TextureArray* pMapResource, const DescriptorCPU* pDsv,
        const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
        const DescriptorGPU* pSrv, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
    );

    const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc() const NOEXCEPT {
        return dsvDesc_;
    }

    const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc() const NOEXCEPT {
        return srvDesc_;
    }

    const DescriptorGPU& srv() const NOEXCEPT {
        return *pSrv_;
    }

    const DescriptorCPU& dsv() const NOEXCEPT {
        return *pDsv_;
    }

    TextureArray& texture() NOEXCEPT {
        return *pMapResource_;
    }

    const TextureArray& texture() const NOEXCEPT {
        return *pMapResource_;
    }

    bool valid() const NOEXCEPT {
        return pMapResource_ != nullptr && pSrv_ != nullptr && pDsv_ != nullptr;
    }

private:
    void onPush(D3D12GfxCmdList& cmdList) override;
    void onBind(D3D12GfxCmdList& cmdList) override;
    void onPop(D3D12GfxCmdList& cmdList) override;
    void onClear(D3D12GfxCmdList& cmdList) override;

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc_;
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc_;
    const DescriptorGPU* pSrv_;
    const DescriptorCPU* pDsv_;
    TextureArray* pMapResource_;
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

    float cascadeLevel[4] = { 0.0f, 40.0f, 100.0f, 1000.0f };

    mu::Mat4x4 MU_CALLCONV calcCascadeViewProj(const Camera& camera, int cascadeLv) const;

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

enum class RenderPassTextures {
    ShadowMap,
};

enum class RenderPassTextureArrays {
    ShadowMapArray,
};

class RenderPass {
public:
    using VBLayoutIdx = std::size_t;
    using ResKey = std::string;

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
    void mapTexture(RenderPassTextures type, Texture* pTexture) {
        textureMap_[type] = pTexture;
    }
    Texture* getTexture(RenderPassTextures type) const {
        auto it = textureMap_.find(type);
        if (it != textureMap_.end()) {
            return it->second;
        }
        return nullptr;
    }

    void mapTextureArray(RenderPassTextureArrays type, TextureArray* pTextureArr) {
        textureArrayMap_[type] = pTextureArr;
    }
    TextureArray* getTextureArray(RenderPassTextureArrays type) const {
        auto it = textureArrayMap_.find(type);
        if (it != textureArrayMap_.end()) {
            return it->second;
        }
        return nullptr;
    }

    virtual std::vector<RenderPassTextures> requiredTextures() const {
        return {};
    }

    virtual std::vector<RenderPassTextureArrays> requiredTextureArrays() const {
        return {};
    }

protected:
    void checkRequiredTextures() const {
        for (const auto& texID : requiredTextures()) {
            if (!textureMap_.contains(texID)) {
                throw std::runtime_error("Required texture not found: " + renderPassID_);
            }
        }
    }

    void checkRequiredTextureArrays() const {
        for (const auto& texID : requiredTextureArrays()) {
            if (!textureArrayMap_.contains(texID)) {
                throw std::runtime_error("Required texture array not found: " + renderPassID_);
            }
        }
    }

private:
    std::unordered_map<RenderPassTextures, Texture*> textureMap_;
    std::unordered_map<RenderPassTextureArrays, TextureArray*> textureArrayMap_;
    std::string renderPassID_;
};

namespace rp {

class PBRIllumination : public gfx::d3d12::RenderPass {
public:
    static constexpr const char* id = "PBRIllumination";

    PBRIllumination( D3D12Device& device, ShaderPBRIllumination& shader,
        const SamplerStorage& samplerStorage, const D3D12_VIEWPORT& vp = D3D12_VIEWPORT{}
    );

    void initResources(
        RenderPassTextureArrays shadowMap,
        const DescriptorCPU* pDsv,
        const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
        const DescriptorGPU* pSrv, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
    );

    std::vector<RenderPassTextureArrays> requiredTextureArrays() const {
        return { 
            RenderPassTextureArrays::ShadowMapArray
        };
    }

    void setViewport(const D3D12_VIEWPORT& vp);

    const D3D12_VIEWPORT& viewport() const NOEXCEPT {
        return viewport_;
    }

    void preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
    void render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
    void postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;

    void trackModel(Model* pModel);
    void trackModel(Model* pModel, const BoundingVolumeNode* pBVNode);
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

    ShadowArrayMaterial shadowArrayMaterial_;
    D3D12_VIEWPORT viewport_;
    RenderProtocol protocol_;
    std::vector<const WorldLight*> lights_;
    std::vector< std::tuple<bool, const BoundingVolumeNode*, Submesh*,
        const coord::System*, VBLayoutIdx, mu::Mat4x4>
    > batch_;
    const Camera* pCamera_;
    const SamplerStorage* pSamplerStorage_;
};

class PBRAnimatedIllumination : public gfx::d3d12::RenderPass {
public:
    static constexpr const char* id = "PBRAnimatedIllumination";

    PBRAnimatedIllumination( D3D12Device& device, ShaderPBRAnimatedIllumination& shader,
        const SamplerStorage& samplerStorage, const D3D12_VIEWPORT& vp = D3D12_VIEWPORT{}
    );

    void initResources(
        RenderPassTextureArrays shadowMap,
        const DescriptorCPU* pDsv,
        const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
        const DescriptorGPU* pSrv, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
    );

    std::vector<RenderPassTextureArrays> requiredTextureArrays() const {
        return {
            RenderPassTextureArrays::ShadowMapArray
        };
    }

    void setViewport(const D3D12_VIEWPORT& vp);

    const D3D12_VIEWPORT& viewport() const NOEXCEPT {
        return viewport_;
    }

    void preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
    void render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
    void postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;

    void trackModel(Model* pModel, const AnimController* pAnimController);
    void trackModel(Model* pModel, const AnimController* pAnimController,
        const BoundingVolumeNode* pBVNode
    );
    void eraseModel(Model* pModel);
    void setCamera(const Camera* pCamera) NOEXCEPT {
        pCamera_ = pCamera;
    }
    void addLight(const WorldLight* pLight) NOEXCEPT {
        lights_.push_back(pLight);
    }

private:
    ShaderPBRAnimatedIllumination& shader() noexcept {
        return static_cast<ShaderPBRAnimatedIllumination&>(protocol_.shader());
    }
    const ShaderPBRAnimatedIllumination& shader() const noexcept {
        return static_cast<const ShaderPBRAnimatedIllumination&>(protocol_.shader());
    }

    static RenderProtocol::Desc makeDesc();

    ShadowArrayMaterial shadowArrayMaterial_;
    D3D12_VIEWPORT viewport_;
    RenderProtocol protocol_;
    std::vector<const WorldLight*> lights_;
    std::vector< std::tuple<bool, const BoundingVolumeNode*, Submesh*,
        const coord::System*, VBLayoutIdx, mu::Mat4x4, const AnimController*>
    > batch_;
    const Camera* pCamera_;
    const SamplerStorage* pSamplerStorage_;
};

class ShadowMap : public gfx::d3d12::RenderPass {
public:
    static constexpr const char* id = "ShadowMap";

    ShadowMap( D3D12Device& device, ShaderShadowMap& shader,
        const D3D12_VIEWPORT& vp = D3D12_VIEWPORT{}
    );

    void initResources(
        const DescriptorCPU* pDsv,
        const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
        const DescriptorGPU* pSrv, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
    );

    std::vector<RenderPassTextures> requiredTextures() const {
        return {RenderPassTextures::ShadowMap};
    }


    void setViewport(const D3D12_VIEWPORT& vp);

    const D3D12_VIEWPORT& viewport() const NOEXCEPT {
        return viewport_;
    }

    void preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
    void render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
    void postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;

    void trackModel(Model* pModel);
    void trackModel(Model* pModel, const BoundingVolumeNode* pBVNode);
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

    ShadowMaterial shadowMaterial_;
    D3D12_VIEWPORT viewport_;
    RenderProtocol protocol_;
    const WorldLight* pLight_;
    std::vector< std::tuple<bool, const BoundingVolumeNode*, Submesh*,
        const coord::System*, VBLayoutIdx, mu::Mat4x4>
    > batch_;
    const Camera* pCamera_;
};

class CascadeShadowMap : public gfx::d3d12::RenderPass {
public:
    static constexpr const char* id = "CascadeShadowMap";

    CascadeShadowMap(D3D12Device& device, ShaderCascadeShadowMap& shader,
        const D3D12_VIEWPORT& vp = D3D12_VIEWPORT{}
    );

    void initResources(
        RenderPassTextureArrays shadowArrayMap, 
        const DescriptorCPU* pDsv,
        const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
        const DescriptorGPU* pSrv, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
    );

    std::vector<RenderPassTextureArrays> requiredTextureArrays() const {
        return { RenderPassTextureArrays::ShadowMapArray
        };
    }


    void setViewport(const D3D12_VIEWPORT& vp);

    const D3D12_VIEWPORT& viewport() const NOEXCEPT {
        return viewport_;
    }

    void preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
    void render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
    void postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;

    void trackModel(Model* pModel);
    void trackModel(Model* pModel, const BoundingVolumeNode* pBVNode);
    void eraseModel(Model* pModel);
    void setCamera(const Camera* pCamera) NOEXCEPT {
        pCamera_ = pCamera;
    }
    void setLight(const WorldLight* pLight);

private:
    ShaderCascadeShadowMap& shader() noexcept {
        return static_cast<ShaderCascadeShadowMap&>(protocol_.shader());
    }
    const ShaderCascadeShadowMap& shader() const noexcept {
        return static_cast<const ShaderCascadeShadowMap&>(protocol_.shader());
    }

    static RenderProtocol::Desc makeDesc();

    ShadowArrayMaterial shadowArrayMaterial_;
    D3D12_VIEWPORT viewport_;
    RenderProtocol protocol_;
    const WorldLight* pLight_;
    std::vector< std::tuple<bool, const BoundingVolumeNode*, Submesh*,
        const coord::System*, VBLayoutIdx, mu::Mat4x4>
    > batch_;
    const Camera* pCamera_;
};

class ShadowMapAnimated : public gfx::d3d12::RenderPass {
public:
    static constexpr const char* id = "ShadowMapAnimated";

    ShadowMapAnimated( D3D12Device& device, ShaderShadowMapAnimated& shader,
        const D3D12_VIEWPORT& vp = D3D12_VIEWPORT{}
    );

    void initResources(
        RenderPassTextures shadowMap, const DescriptorCPU* pDsv,
        const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
        const DescriptorGPU* pSrv, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
    );

    std::vector<RenderPassTextures> requiredTextures() const {
        return {RenderPassTextures::ShadowMap};
    }

    void setViewport(const D3D12_VIEWPORT& vp);

    const D3D12_VIEWPORT& viewport() const NOEXCEPT {
        return viewport_;
    }

    void preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
    void render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
    void postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;

    void trackModel(Model* pModel, const AnimController* pAnimController);
    void trackModel(Model* pModel, const AnimController* pAnimController,
        const BoundingVolumeNode* pBVNode
    );
    void setCamera(const Camera* pCamera) NOEXCEPT {
        pCamera_ = pCamera;
    }
    void setLight(const WorldLight* pLight);

private:
    ShaderShadowMapAnimated& shader() noexcept {
        return static_cast<ShaderShadowMapAnimated&>(protocol_.shader());
    }
    const ShaderShadowMapAnimated& shader() const noexcept {
        return static_cast<const ShaderShadowMapAnimated&>(protocol_.shader());
    }

    static RenderProtocol::Desc makeDesc();

    ShadowMaterial shadowMaterial_;
    D3D12_VIEWPORT viewport_;
    RenderProtocol protocol_;
    const WorldLight* pLight_;
    std::vector< std::tuple<bool, const BoundingVolumeNode*, Submesh*,
        const coord::System*, VBLayoutIdx, mu::Mat4x4, const AnimController*>
    > batch_;
    const Camera* pCamera_;
};

//class CascadeShadowMapAnimated : public gfx::d3d12::RenderPass {
//public:
//    static constexpr const char* id = "CascadeShadowMapAnimated";
//
//    CascadeShadowMapAnimated( D3D12Device& device, ShaderCascadeShadowMapAnimated& shader,
//        const D3D12_VIEWPORT& vp = D3D12_VIEWPORT{}
//    );
//
//    void initResources(
//        RenderPassTextureArrays shadowMap, 
//        const DescriptorCPU* pDsv,
//        const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
//        const DescriptorGPU* pSrv, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
//    );
//
//    std::vector<RenderPassTextureArrays> requiredTextureArrays() const {
//        return {
//          RenderPassTextureArrays::ShadowMapArray
//        };
//    }
//
//    void setViewport(const D3D12_VIEWPORT& vp);
//
//    const D3D12_VIEWPORT& viewport() const NOEXCEPT {
//        return viewport_;
//    }
//
//    void preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
//    void render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
//    void postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
//
//    void trackModel(Model* pModel, const AnimController* pAnimController);
//    void trackModel(Model* pModel, const AnimController* pAnimController,
//        const BoundingVolumeNode* pBVNode
//    );
//    void eraseModel(Model* pModel);
//    void setCamera(const Camera* pCamera) NOEXCEPT {
//        pCamera_ = pCamera;
//    }
//    void setLight(const WorldLight* pLight);
//
//private:
//    ShaderCascadeShadowMapAnimated& shader() noexcept {
//        return static_cast<ShaderCascadeShadowMapAnimated&>(protocol_.shader());
//    }
//    const ShaderCascadeShadowMapAnimated& shader() const noexcept {
//        return static_cast<const ShaderCascadeShadowMapAnimated&>(protocol_.shader());
//    }
//
//    static RenderProtocol::Desc makeDesc();
//
//    ShadowArrayMaterial shadowMaterial_;
//    D3D12_VIEWPORT viewport_;
//    RenderProtocol protocol_;
//    const WorldLight* pLight_;
//    std::vector< std::tuple<bool, const BoundingVolumeNode*, Submesh*,
//        const coord::System*, VBLayoutIdx, mu::Mat4x4, const AnimController*>
//    > batch_;
//    const Camera* pCamera_;
//};

class ScreenQuad : public gfx::d3d12::RenderPass {
public:
    static constexpr const char* id = "ScreenQuad";

    ScreenQuad( D3D12Device& device, ShaderScreenQuad& shader,
        const SamplerStorage& samplerStorage,
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
    const SamplerStorage* pSamplerStorage_;
};

class Tessellation : public gfx::d3d12::RenderPass {
public:
    static constexpr const char* id = "Tessellation";

    Tessellation( D3D12Device& device, ShaderTessellation& shader,
        const SamplerStorage& samplerStorage,
        const D3D12_VIEWPORT& vp = D3D12_VIEWPORT{}
    );

    void initResources(
        RenderPassTextureArrays shadowMap,
        const DescriptorCPU* pDsv,
        const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
        const DescriptorGPU* pSrv, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
    );

    std::vector<RenderPassTextureArrays> requiredTextureArrays() const {
        return { 
          RenderPassTextureArrays::ShadowMapArray
        };
    }

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

    ShadowArrayMaterial shadowArrayMaterial_;
    D3D12_VIEWPORT viewport_;
    RenderProtocol protocol_;
    std::vector<const WorldLight*> lights_;
    std::vector<const LevelChunkModel*> batch_;
    const Camera* pCamera_;
    const SamplerStorage* pSamplerStorage_;
};

class ShadowMapTessellation : public gfx::d3d12::RenderPass {
public:
    static constexpr const char* id = "ShadowMapTessellation";

    ShadowMapTessellation( D3D12Device& device, ShaderShadowMapTessellation& shader,
        const SamplerStorage& samplerStorage,
        const D3D12_VIEWPORT& vp = D3D12_VIEWPORT{}
    );

    void initResources(
        RenderPassTextureArrays shadowArrayMap,
        const DescriptorCPU* pDsv,
        const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
        const DescriptorGPU* pSrv, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
    );

    std::vector<RenderPassTextureArrays> requiredTextureArrays() const {
        return {
          RenderPassTextureArrays::ShadowMapArray
        };
    }

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
    void setLight(const WorldLight* pLight);

private:
    ShaderShadowMapTessellation& shader() noexcept {
        return static_cast<ShaderShadowMapTessellation&>(protocol_.shader());
    }
    const ShaderShadowMapTessellation& shader() const noexcept {
        return static_cast<const ShaderShadowMapTessellation&>(protocol_.shader());
    }

    static RenderProtocol::Desc makeDesc();

    ShadowArrayMaterial shadowArrayMaterial_;
    D3D12_VIEWPORT viewport_;
    RenderProtocol protocol_;
    const WorldLight* pLight_;
    std::vector<const LevelChunkModel*> batch_;
    const Camera* pCamera_;
    const SamplerStorage* pSamplerStorage_;
};

class Skybox : public gfx::d3d12::RenderPass {
public:
    static constexpr const char* id = "Skybox";

    Skybox(D3D12Device& device, ShaderSkybox& shader,
        const SamplerStorage& samplerStorage, const D3D12_VIEWPORT& vp = D3D12_VIEWPORT{}
    );

    void setViewport(const D3D12_VIEWPORT& vp);

    const D3D12_VIEWPORT& viewport() const NOEXCEPT {
        return viewport_;
    }

    void preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
    void render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;
    void postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) override;

    void setCamera(const Camera* pCamera) NOEXCEPT {
        pCamera_ = pCamera;
    }

private:
    ShaderSkybox& shader() noexcept {
        return static_cast<ShaderSkybox&>(protocol_.shader());
    }
    const ShaderSkybox& shader() const noexcept {
        return static_cast<const ShaderSkybox&>(protocol_.shader());
    }

    static RenderProtocol::Desc makeDesc();

    D3D12_VIEWPORT viewport_;
    RenderProtocol protocol_;
    std::vector< std::tuple<bool, const BoundingVolumeNode*, Submesh*,
        const coord::System*, VBLayoutIdx, mu::Mat4x4>
    > batch_;
    const Camera* pCamera_;
    const SamplerStorage* pSamplerStorage_;
};

}   // namespace gfx::d3d12::rp

}   // namespace gfx::d3d12

}   // namespace gfx

#endif  // __d3d12RenderPass_HPP