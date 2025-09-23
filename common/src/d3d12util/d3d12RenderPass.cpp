#include "d3d12util/d3d12RenderPass.hpp"

#include "game/animSystem.hpp"

#include <ranges>
#include <algorithm>
#include <tuple>
#include <unordered_map>

namespace gfx {


namespace d3d12 {

void Camera::updateView() {
    repPos_ = mu::Vec3(coordMovement_.xform().row(3));
    if (focusMode_ == FocusMode::None) {
        repUp_ = mu::Vec3(coordRotation_.xform().row(1));
    }

    switch (focusMode_) {
    case FocusMode::LookAt:
        repFwd_ = mu::normalize(focus_ - repPos_);
        view_ = mu::lookAt(repPos_, focus_, repUp_);
        break;

    case FocusMode::LookTo:
        repFwd_ = mu::normalize(focus_);
        view_ = mu::lookAt(repPos_, repPos_ + repFwd_, repUp_);
        break;

    case FocusMode::None:
        repFwd_ = mu::normalize(mu::Vec3(coordRotation_.xform().row(2)));
        view_ = mu::lookAt(repPos_, repPos_ + repFwd_, repUp_);
        break;
    }
}

void RenderTargets::pushTarget(D3D12GfxCmdList& cmdList, Specifier spec, IRenderTarget* pTarget) {
    if (map_.contains(spec)) {
        throw GFX_EXCEPT( std::string("RenderTarget with the specifier")
            + sSpecifierStrings[etoi(spec)] + " already exists."
        );
    }
    map_[spec] = pTarget;
    pTarget->onPush(cmdList);
}

IRenderTarget* RenderTargets::popTarget(D3D12GfxCmdList& cmdList, Specifier spec) {
    if (!map_.contains(spec)) {
        throw GFX_EXCEPT( std::string("RenderTarget with the specifier")
            + sSpecifierStrings[etoi(spec)] + " does not exist."
        );
    }
    auto itTarget = map_.find(spec);
    auto pTarget = itTarget->second;
    map_.erase(itTarget);
    pTarget->onPop(cmdList);
    return pTarget;
}

std::string RenderTargets::sSpecifierStrings[etoi(RenderTargets::Specifier::SIZE)] = {
    "Main", "Shadow"
};

ShadowMaterial::ShadowMaterial()
    : dsvDesc_{}, srvDesc_{}, pSrv_(nullptr), pDsv_(nullptr), pMapResource_(nullptr) {}

ShadowMaterial::ShadowMaterial( Texture* pMapResource,
    const DescriptorCPU* pDsv, const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
    const DescriptorGPU* pSrv, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
) : dsvDesc_(dsvDesc), srvDesc_(srvDesc), pSrv_(pSrv), pDsv_(pDsv),
    pMapResource_(pMapResource) {
    addMapRef(MapType::Shadow, MapRef{
        .type = etoi(ResourceType::Texture),
        .resourceIdx = static_cast<std::uint32_t>( pSrv_->offset() ),
    } );
}

void ShadowMaterial::onPush(D3D12GfxCmdList& cmdList) {
    pMapResource_->commitState(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

void ShadowMaterial::onPop(D3D12GfxCmdList& cmdList) {
    pMapResource_->commitState(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void ShadowMaterial::onBind(D3D12GfxCmdList& cmdList) {
    cmdList.get()->OMSetRenderTargets(0u, nullptr, false, &pDsv_->cpuHandle());
}

void ShadowMaterial::onClear(D3D12GfxCmdList& cmdList) {
    cmdList.get()->ClearDepthStencilView(
        pDsv_->cpuHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0u, 0u, nullptr
    );
}

ShadowArrayMaterial::ShadowArrayMaterial()
    : dsvDesc_{}, srvDesc_{}, pSrv_(nullptr), pDsv_(nullptr), pMapResource_(nullptr) {
}

ShadowArrayMaterial::ShadowArrayMaterial(TextureArray* pMapResource,
    const DescriptorCPU* pDsv, const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
    const DescriptorGPU* pSrv, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
) : dsvDesc_(dsvDesc), srvDesc_(srvDesc), pSrv_(pSrv), pDsv_(pDsv),
pMapResource_(pMapResource) {
    addMapRef(MapType::ShadowCascade0, MapRef{
        .type = etoi(ResourceType::TextureArray),
        .resourceIdx = static_cast<std::uint32_t>(pSrv_->offset()),
        .arrayIdx = 0u
        });
    addMapRef(MapType::ShadowCascade1, MapRef{
        .type = etoi(ResourceType::TextureArray),
        .resourceIdx = static_cast<std::uint32_t>(pSrv_->offset()),
        .arrayIdx = 1u
        });
    addMapRef(MapType::ShadowCascade2, MapRef{
        .type = etoi(ResourceType::TextureArray),
        .resourceIdx = static_cast<std::uint32_t>(pSrv_->offset()),
        .arrayIdx = 2u
        });
}

void ShadowArrayMaterial::onPush(D3D12GfxCmdList& cmdList) {
    pMapResource_->commitState(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
}

void ShadowArrayMaterial::onPop(D3D12GfxCmdList& cmdList) {
    pMapResource_->commitState(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void ShadowArrayMaterial::onBind(D3D12GfxCmdList& cmdList) {
    cmdList.get()->OMSetRenderTargets(0u, nullptr, false, &pDsv_->cpuHandle());
}

void ShadowArrayMaterial::onClear(D3D12GfxCmdList& cmdList) {
    cmdList.get()->ClearDepthStencilView(
        pDsv_->cpuHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0u, 0u, nullptr
    );
}

mu::Mat4x4 MU_CALLCONV WorldLight::calcCascadeViewProj(const Camera& camera, int cascadeLv) const
{
    // 시야각을 이용해서 수직 시야각을 구함
    auto fov = mu::Radian(camera.defFov);

    float tanHalfVFov = tanf( float(fov) * 0.5f );

    // 수직 시야각을 이용해서 수평 시야각을 구함
    float tanHalfHFov = tanHalfVFov * camera.defAspect;

    // 절두체를 나누기 위한 각 부분 절두체의 끝 지점 선언
    float cascadeEnd[4] = { camera.defNear, cascadeLevel[1], cascadeLevel[2], camera.defFar};

    // +X, +Y 좌표에 수평, 수직 시야각을 이용하여 구함. 각 부분 절두체의 가까운, 먼 평면의 값을 곱하여 4개의 점을 구함
    float xn = cascadeEnd[cascadeLv] * tanHalfHFov;
    float xf = cascadeEnd[cascadeLv + 1] * tanHalfHFov;
    float yn = cascadeEnd[cascadeLv] * tanHalfVFov;
    float yf = cascadeEnd[cascadeLv + 1] * tanHalfVFov;

    // 4개의 점을 이용하여 절두체의 8개의 점을 구함
    mu::Vec4 frustumCorner[8] = {
        mu::Vec4(xn, yn, cascadeEnd[cascadeLv], 1.0f),
        mu::Vec4(-xn, yn, cascadeEnd[cascadeLv], 1.0f),
        mu::Vec4(xn, -yn, cascadeEnd[cascadeLv], 1.0f),
        mu::Vec4(-xn, -yn, cascadeEnd[cascadeLv], 1.0f),
        mu::Vec4(xf, yf, cascadeEnd[cascadeLv + 1], 1.0f),
        mu::Vec4(-xf, yf, cascadeEnd[cascadeLv + 1], 1.0f),
        mu::Vec4(xf, -yf, cascadeEnd[cascadeLv + 1], 1.0f),
        mu::Vec4(-xf, -yf, cascadeEnd[cascadeLv + 1], 1.0f)
    };    

    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::min();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::min();
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::min();

    for (int j = 0; j < 8; ++j)
    {
        minX = std::min(minX, frustumCorner[j].x());
        maxX = std::max(maxX, frustumCorner[j].x());
        minY = std::min(minY, frustumCorner[j].y());
        maxY = std::max(maxY, frustumCorner[j].y());
        minZ = std::min(minZ, frustumCorner[j].z());
        maxZ = std::max(maxZ, frustumCorner[j].z());
    }

    // frustum center
    mu::Vec4 frustumCenter = mu::Vec4(
        (minX + maxX) * 0.5f,
        (minY + maxY) * 0.5f,
        (minZ + maxZ) * 0.5f,
        1.0f
    );

    mu::Mat4x4 fuck = mu::transpose( mu::lookAt(
        mu::Vec3(),
        dir,
        mu::Vec3(0.0f, 1.0f, 0.0f)
    ) );

    frustumCenter *= fuck;

    // mu::Vec3 cameraTranslation{};
    mu::Vec3 cameraTranslation = camera.repPos();
    frustumCenter += mu::Vec4(cameraTranslation, 0.0f);

    mu::Vec3 lightPos = mu::Vec3(frustumCenter.x(), frustumCenter.y(), frustumCenter.z());
    lightPos -= dir * (cascadeEnd[cascadeLv] + (cascadeEnd[cascadeLv + 1] - cascadeEnd[cascadeLv]) / 2.f) * 2.f;
    // lightPos -= camera.repFwd() * (maxY - minY) / 4.f;

    // Light view 행렬 계산 (lookAt 방식)
    mu::Mat4x4 lightView = mu::lookAt(
        lightPos,
        lightPos + dir,
        mu::Vec3(0.0f, 1.0f, 0.0f)
    );

    // Light projection 행렬 계산
    mu::Mat4x4 lightProj = mu::ortho(
        minX, maxX,
        minY, maxY,
        0.1f, maxZ * 2.f
    );
    
    return lightView * lightProj;
}

sr::Light WorldLight::toViewLight(const Camera& camera) const {
    const auto view = camera.view();
    const auto viewForVector = mu::transpose( mu::inverse(view) );

    return sr::Light{
        .color = color.getXmf(),
        .falloff = falloff,
        .posV = mu::Vec3( mu::Vec4(pos, 1.0f) * view ).getXmf(),
        .cosTheta = cosTheta,
        .dirV = mu::Vec3( mu::Vec4(dir, 0.0f) * viewForVector ).getXmf(),
        .cosPhi = cosPhi,
        .atten = atten.getXmf(),
        .intensity = intensity,
        .type = etoi(type)
    };
}

mu::Mat4x4 MU_CALLCONV WorldLight::view(const Camera& camera) const {
    const auto lightPos = mu::Vec3( camera.repPos() - dir * distanceToCamera );
    const auto lightDir = dir;
    // we assume that the light should have up vector (0, 1, 0)

    return mu::lookAt( lightPos, lightPos + lightDir, mu::Vec3( 0.f, 1.f, 0.f ) );
}

mu::Mat4x4 MU_CALLCONV WorldLight::proj() const {
    if (type == Type::Directional) {
        return mu::ortho( config.ortho.width, config.ortho.height,
            config.ortho.nearZ, config.ortho.farZ
        );
    } else {
        return mu::persp( config.perspective.fovy, config.perspective.aspect,
            config.perspective.nearZ, config.perspective.farZ
        );
    }

    throw GFX_EXCEPT( "[Description]: Invalid light type." );
}

mu::Mat4x4 MU_CALLCONV WorldLight::viewProj(const Camera& camera) const {
    return view(camera) * proj();
}

namespace rp {

PBRIllumination::PBRIllumination( D3D12Device& device, ShaderPBRIllumination& shader, 
    const SamplerStorage& samplerStorage, const D3D12_VIEWPORT& vp
) : gfx::d3d12::RenderPass(id),
    shadowArrayMaterial_(), viewport_(vp),
    protocol_( shader.makeProtocol( device,
        RenderProtocol::Desc{ makeDesc() }
    ) ), lights_(), batch_(), pidCache_(), lightCache_(), pCamera_(nullptr),
    pSamplerStorage_(&samplerStorage) {
    auto pcd = sr::PerConfigurationData0{
        .viewportWidth = vp.Width,
        .viewportHeight = vp.Height
    };

    shader.perConfigurationData_.stage(&pcd, sizeof(sr::PerConfigurationData0));
}

void PBRIllumination::initResources(
    RenderPassTextureArrays shadowArrayMap,
    const DescriptorCPU* pDsv,
    const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
    const DescriptorGPU* pSrv, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
) {
    checkRequiredTextureArrays();
    shadowArrayMaterial_ = ShadowArrayMaterial(
        getTextureArray(shadowArrayMap),
        pDsv, dsvDesc, pSrv, srvDesc
    );
}

RenderProtocol::Desc PBRIllumination::makeDesc() {
    return RenderProtocol::Desc {
        .blend = D3D12_BLEND_DESC{
            .AlphaToCoverageEnable = false,
            .IndependentBlendEnable = false,
            .RenderTarget = {
                D3D12_RENDER_TARGET_BLEND_DESC{
                    .BlendEnable = false,
                    .LogicOpEnable = false,
                    .SrcBlend = D3D12_BLEND_ONE,
                    .DestBlend = D3D12_BLEND_ZERO,
                    .BlendOp = D3D12_BLEND_OP_ADD,
                    .SrcBlendAlpha = D3D12_BLEND_ONE,
                    .DestBlendAlpha = D3D12_BLEND_ZERO,
                    .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                    .LogicOp = D3D12_LOGIC_OP_NOOP,
                    .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
                }
            }
        },
        .sampleMask = UINT_MAX,
        .rasterizerState = D3D12_RASTERIZER_DESC{
            .FillMode = D3D12_FILL_MODE_SOLID,
            .CullMode = D3D12_CULL_MODE_BACK,
            .DepthClipEnable = true
        },
        .depthStencilState = D3D12_DEPTH_STENCIL_DESC{
            .DepthEnable = true,
            .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
            .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
            .StencilEnable = false
        },
        .primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .numRenderTargets = 1u,
        .rtvFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
        .dsvFormat = DXGI_FORMAT_D32_FLOAT,
        .sampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
        .nodeMask = 0u
    };
}

void PBRIllumination::setViewport(const D3D12_VIEWPORT& vp) {
    viewport_ = vp;

    auto pcd = sr::PerConfigurationData0{
        .viewportWidth = vp.Width,
        .viewportHeight = vp.Height
    };

    shader().perConfigurationData_.stage(&pcd, sizeof(sr::PerConfigurationData0));
}


void PBRIllumination::preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    cmdList.get()->SetPipelineState( protocol_.get().Get() );
    cmdList.get()->RSSetViewports(1u, &viewport_);
    auto scissorRect = D3D12_RECT{ 0, 0, static_cast<LONG>(viewport_.Width), static_cast<LONG>(viewport_.Height) };
    cmdList.get()->RSSetScissorRects(1u, &scissorRect);

    // firstly sort batch by bounding volume node and other properties
    // to cull out the same bounding volume nodes
    // that are out of the camera frustum.
    auto proj = [this](const auto& tuple) {
        return std::tuple(
            std::get<const BoundingVolumeNode*>(tuple),
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
    };

    std::ranges::sort(batch_, std::less<>{}, proj);

    // auto pidCache_ = std::vector<sr::PerInstanceData0>();
    pidCache_.clear();
    pidCache_.reserve( shader().maxInstanceCnt() );

    const BoundingVolumeNode* pLastCulledBVNode = nullptr;

    for (auto& [willNotDraw, pBVNode, pSubmesh, pCoord, vbLayoutIdx, xform] : batch_) {
        // update xform
        xform = pSubmesh->parent()->parent()->coord().xform();

        // cull out instances that are out of the camera frustum.
        // enhance performance by eliding collision check of the same bounding volume nodes.
        if (pLastCulledBVNode && pLastCulledBVNode == pBVNode) {
            willNotDraw = true;
            continue;
        }

        // if (pBVNode && !BoundingVolumeNode::collides(
        //     pCamera_->coordRotation().xform(), pCamera_->bvNode(),
        //     pCoord->xform(), *pBVNode
        // )) {
        //     willNotDraw = true;
        //     pLastCulledBVNode = pBVNode;
        //     continue;
        // }

        willNotDraw = false;
    }

    // finally sort batch by culled status and other properties
    // to separate instances that are visible and invisible.
    auto proj2 = [this](const auto& tuple) {
        return std::tuple(
            std::get<bool>(tuple),
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
    };

    std::ranges::sort(batch_, std::less<>{}, proj2);

    for (const auto& [willNotDraw, pBVNode, pSubmesh, pCoord, vbLayoutIdx, xform] : batch_) {
        // as the first criterion of sorting is the culled status,
        // if the first instance is culled, then all the rest are culled.
        if (willNotDraw) {
            break;
        }

        // upload per instance data
        pidCache_.emplace_back(
            /* .wvp = */ mu::transpose( xform * pCamera_->view() * pCamera_->proj() ).getXmf(),
            /* .world = */ mu::transpose( xform ).getXmf(),
            /* .wv = */ mu::transpose( xform * pCamera_->view() ).getXmf(),
            /* .wvNormal = */ dx::convertMat<dx::XMFLOAT3X3>(
                mu::inverse(xform * pCamera_->view()).get()
            )
        );

        if (pidCache_.size() == shader().maxInstanceCnt()) [[unlikely]] {
            break;
        }
    }


    // auto lightCache_ = std::vector<sr::Light>();
    lightCache_.clear();
    lightCache_.reserve( lights_.size() );

    for (const auto& light : lights_) {
        lightCache_.emplace_back( light->toViewLight(*pCamera_) );

        if (lightCache_.size() == shader().maxLightCnt()) [[unlikely]] {
            break;
        }
    }


    shadowArrayMaterial_.texture().commitState(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    

    // temporarilly directional light at first
    const auto pDirectionalLight = lights_.front();

    const auto pfd = sr::PerFrameData2{
        .globalAmbient = dx::XMFLOAT3(0.1f, 0.1f, 0.1f),
        .shadowMapRef = {
            shadowArrayMaterial_.mapRef(Material::MapType::ShadowCascade0).toxm(),
            shadowArrayMaterial_.mapRef(Material::MapType::ShadowCascade1).toxm(),
            shadowArrayMaterial_.mapRef(Material::MapType::ShadowCascade2).toxm()
        },
        .lightVP = {
            mu::transpose(pDirectionalLight->calcCascadeViewProj(*pCamera_, 0)).getXmf(),
            mu::transpose(pDirectionalLight->calcCascadeViewProj(*pCamera_, 1)).getXmf(),
            mu::transpose(pDirectionalLight->calcCascadeViewProj(*pCamera_, 2)).getXmf()
        },
        .lightCnt = static_cast<std::uint32_t>( lightCache_.size() )
    };

    shader().perInstanceData_.stage(pidCache_.data(), pidCache_.size() * sizeof(sr::PerInstanceData0));
    shader().lightBuffer_.stage(lightCache_.data(), lightCache_.size() * sizeof(sr::Light));
    shader().perFrameData_.stage(&pfd, sizeof(sr::PerFrameData2));
}

void PBRIllumination::render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    renderTargets.bind(cmdList, RenderTargets::Specifier::Main);

    auto first = batch_.begin();
    auto accDrawcallCnt = 0u;

    // instances that are going to be drawn are at the front of the batch
    // as the first criterion of sorting is the culled status.
    auto proj = [this](const auto& tuple) {
        return std::tuple(
            std::get<bool>(tuple),
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
    };

    while (first != batch_.end()) {
        auto last = std::ranges::upper_bound(first, batch_.end(), proj(*first), std::less<>{}, proj);

        // as the first criterion of sorting is the culled status,
        // if the first instance is culled, then all the rest are culled.
        auto willNotDraw = std::get<bool>(*first);
        if (willNotDraw) {
            break;
        }

        auto pSubmesh = std::get<gfx::d3d12::Submesh*>(*first);

        auto material = sr::PBRMaterial::convert( pSubmesh->material() );

        auto pdd = sr::PerDrawcallData0{
            .material = material,
            .instanceBase = static_cast<std::uint32_t>(first - batch_.begin()),
            .samplerIdx = pSamplerStorage_->get( SamplerStorage::Indices::TrilinearBorder ).index(),
            .shadowSamplerIdx = pSamplerStorage_->get( SamplerStorage::Indices::BilinearComparison ).index(),
        };
        shader().perDrawcallData_.stage( &pdd, sizeof(sr::PerDrawcallData0),
            0u, accDrawcallCnt * shader().cbDrawcallDataSize()
        );

        shader().bindPerDrawcallData(accDrawcallCnt++, cmdList);

        shader().draw( cmdList, *pSubmesh, static_cast<std::size_t>(last - first),
            std::get<VBLayoutIdx>(*first)
        );

        if (accDrawcallCnt == shader().maxDrawcallCnt()) {
            break;
        }

        first = last;
    }
}

void PBRIllumination::postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    shadowArrayMaterial_.texture().commitState(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE); 
}

void PBRIllumination::trackModel(Model* pModel) {
    if (!pModel->markedRenderPasses().empty()) {
        auto it = std::ranges::find(pModel->markedRenderPasses(), renderPassID());
        if (it == pModel->markedRenderPasses().end()) {
            return;
        }
    }

    auto& nodes = pModel->nodes();
    const auto* pCoord = &pModel->root()->coord();

    for (auto& node : nodes) {
        auto xform = node.coord().xform();
        for (auto& mesh : node.meshes()) {
            auto vbLayoutIdx = protocol_.compatibleLayout(*mesh.refMesh());
            if (!vbLayoutIdx) {
                throw std::runtime_error("Incompatible mesh");
            }

            for (auto& submesh : mesh.submeshes()) {
                batch_.emplace_back(false, nullptr, &submesh, pCoord, vbLayoutIdx.value(), xform);
            }
        }
    }
}

void PBRIllumination::trackModel(Model* pModel, const BoundingVolumeNode* pBVNode) {
    if (!pModel->markedRenderPasses().empty()) {
        auto it = std::ranges::find(pModel->markedRenderPasses(), renderPassID());
        if (it == pModel->markedRenderPasses().end()) {
            return;
        }
    }

    auto& nodes = pModel->nodes();
    const auto* pCoord = &pModel->root()->coord();

    for (auto& node : nodes) {
        auto xform = node.coord().xform();
        for (auto& mesh : node.meshes()) {
            auto vbLayoutIdx = protocol_.compatibleLayout(*mesh.refMesh());
            if (!vbLayoutIdx) {
                throw std::runtime_error("Incompatible mesh");
            }

            for (auto& submesh : mesh.submeshes()) {
                batch_.emplace_back(false, pBVNode, &submesh, pCoord, vbLayoutIdx.value(), xform);
            }
        }
    }
}

PBRAnimatedIllumination::PBRAnimatedIllumination(
    D3D12Device& device, ShaderPBRAnimatedIllumination& shader,
    const SamplerStorage& samplerStorage, const D3D12_VIEWPORT& vp
) : gfx::d3d12::RenderPass(id),
    shadowArrayMaterial_(), viewport_(vp),
    protocol_( shader.makeProtocol( device,
        RenderProtocol::Desc{ makeDesc() }
    ) ), lights_(), batch_(), pidCache_(), lightCache_(), boneCache_(),
    pCamera_(nullptr), pSamplerStorage_(&samplerStorage) {
    auto pcd = sr::PerConfigurationData0{
        .viewportWidth = vp.Width,
        .viewportHeight = vp.Height
    };

    shader.perConfigurationData_.stage(&pcd, sizeof(sr::PerConfigurationData0));
}

void PBRAnimatedIllumination::initResources(
    RenderPassTextureArrays shadowArrayMap,
    const DescriptorCPU* pDsv,
    const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
    const DescriptorGPU* pSrv, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
) {
    checkRequiredTextureArrays();
    shadowArrayMaterial_ = ShadowArrayMaterial(
        getTextureArray(shadowArrayMap),
        pDsv, dsvDesc, pSrv, srvDesc
    );
}

RenderProtocol::Desc PBRAnimatedIllumination::makeDesc() {
    return RenderProtocol::Desc {
        .blend = D3D12_BLEND_DESC{
            .AlphaToCoverageEnable = false,
            .IndependentBlendEnable = false,
            .RenderTarget = {
                D3D12_RENDER_TARGET_BLEND_DESC{
                    .BlendEnable = false,
                    .LogicOpEnable = false,
                    .SrcBlend = D3D12_BLEND_ONE,
                    .DestBlend = D3D12_BLEND_ZERO,
                    .BlendOp = D3D12_BLEND_OP_ADD,
                    .SrcBlendAlpha = D3D12_BLEND_ONE,
                    .DestBlendAlpha = D3D12_BLEND_ZERO,
                    .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                    .LogicOp = D3D12_LOGIC_OP_NOOP,
                    .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
                }
            }
        },
        .sampleMask = UINT_MAX,
        .rasterizerState = D3D12_RASTERIZER_DESC{
            .FillMode = D3D12_FILL_MODE_SOLID,
            .CullMode = D3D12_CULL_MODE_BACK,
            .DepthClipEnable = true
        },
        .depthStencilState = D3D12_DEPTH_STENCIL_DESC{
            .DepthEnable = true,
            .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
            .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
            .StencilEnable = false
        },
        .primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .numRenderTargets = 1u,
        .rtvFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
        .dsvFormat = DXGI_FORMAT_D32_FLOAT,
        .sampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
        .nodeMask = 0u
    };
}

void PBRAnimatedIllumination::setViewport(const D3D12_VIEWPORT& vp) {
    viewport_ = vp;

    auto pcd = sr::PerConfigurationData0{
        .viewportWidth = vp.Width,
        .viewportHeight = vp.Height
    };

    shader().perConfigurationData_.stage(&pcd, sizeof(sr::PerConfigurationData0));
}

void PBRAnimatedIllumination::preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    cmdList.get()->SetPipelineState( protocol_.get().Get() );
    cmdList.get()->RSSetViewports(1u, &viewport_);
    auto scissorRect = D3D12_RECT{ 0, 0, static_cast<LONG>(viewport_.Width), static_cast<LONG>(viewport_.Height) };
    cmdList.get()->RSSetScissorRects(1u, &scissorRect);

    // firstly sort batch by bounding volume node and other properties
    // to cull out the same bounding volume nodes
    // that are out of the camera frustum.
    auto proj = [this](const auto& tuple) {
        return std::tuple(
            std::get<const BoundingVolumeNode*>(tuple),
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
    };

    std::ranges::sort(batch_, std::less<>{}, proj);

    const BoundingVolumeNode* pLastCulledBVNode = nullptr;

    for (auto& [willNotDraw, pBVNode, pSubmesh, pCoord, vbLayoutIdx, xform, pAnimCon] : batch_) {
        // update xform
        xform = pSubmesh->parent()->parent()->model()->root()->coord().xform();

        // cull out instances that are out of the camera frustum.
        // enhance performance by eliding collision check of the same bounding volume nodes.
        if (pLastCulledBVNode && pLastCulledBVNode == pBVNode) {
            willNotDraw = true;
            continue;
        }

        // if (pBVNode && !BoundingVolumeNode::collides(
        //     pCamera_->coordRotation().xform(), pCamera_->bvNode(),
        //     pCoord->xform(), *pBVNode
        // )) {
        //     willNotDraw = true;
        //     pLastCulledBVNode = pBVNode;
        //     continue;
        // }

        willNotDraw = false;
    }

    // finally sort batch by culled status and other properties
    // to separate instances that are visible and invisible.
    auto proj2 = [this](const auto& tuple) {
        return std::tuple(
            std::get<bool>(tuple),
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
    };

    std::ranges::sort(batch_, std::less<>{}, proj2);

    // auto pidCache_ = std::vector<sr::PerInstanceData5>();
    pidCache_.clear();
    pidCache_.reserve( shader().maxInstanceCnt() );

    // auto boneCache_ = std::vector<dx::XMFLOAT4X4>();
    boneCache_.clear();
    boneCache_.reserve( shader().maxBoneCnt() );

    static constexpr std::size_t targetAnimCntExpected = 4u;

    for (const auto& [willNotDraw, pBVNode, pSubmesh, pCoord, vbLayoutIdx, xform, pAnimCon] : batch_) {
        // as the first criterion of sorting is the culled status,
        // if the first instance is culled, then all the rest are culled.
        if (willNotDraw) {
            break;
        }

        auto firstBoneIndices = std::vector<std::uint32_t>();
        firstBoneIndices.reserve( targetAnimCntExpected );
        auto weights = std::vector<float>();
        weights.reserve( targetAnimCntExpected );
        auto clipmodes = std::vector<AnimInstance::ClipMode>();
        clipmodes.reserve( targetAnimCntExpected );
        auto sampleIndices = std::vector<int>();
        sampleIndices.reserve( targetAnimCntExpected );
        auto animIndices = std::vector<std::uint32_t>();
        animIndices.reserve( targetAnimCntExpected );

        bool boneFull = false;
        std::size_t boneCntInSkeleton = pAnimCon->skeleton().bones().size();

        for (const auto& [key, animInst] : pAnimCon->instances()) {
            weights.push_back(animInst.weight());

            if (clipmodes.emplace_back(animInst.clipMode()) == AnimInstance::ClipMode::KeyFrame) {
                firstBoneIndices.push_back(static_cast<std::uint32_t>(boneCache_.size()));
                animIndices.push_back(firstBoneIndices.back());

                for (const auto& bone: animInst.boneXformCache()) {
                    boneCache_.push_back( mu::transpose(bone).getXmf() );
                }

                if (boneCache_.size() == shader().maxBoneCnt()) {
                    boneFull = true;
                    break;
                }
            }
            else /* if (animInst.clipMode() == AnimInstance::ClipMode::Presampled) */ {
                auto bakedXforms = static_cast<Texture*>(animInst.animClip()->customData());
                if (!bakedXforms) {
                    throw GFX_EXCEPT( "[Description]: Presampled animation clip has no baked xforms." );
                }

                animIndices.push_back(static_cast<std::uint32_t>(bakedXforms->view(bakedXforms->idxSrv).offset()));
                sampleIndices.push_back( static_cast<int>(
                    animInst.elapsed() / animInst.animClip()->sampleInterval()
                ) );
            }
            
        }

        if (boneFull) {
            break;
        }


        // upload per instance data
        pidCache_.emplace_back(
            /* .wvp = */ mu::transpose( xform * pCamera_->view() * pCamera_->proj() ).getXmf(),
            /* .world = */ mu::transpose( xform ).getXmf(),
            /* .wv = */ mu::transpose( xform * pCamera_->view() ).getXmf(),
            /* .wvNormal = */ dx::convertMat<dx::XMFLOAT3X3>(
                mu::inverse(xform * pCamera_->view()).get()
            ),
            /* .animIdx0 = */ (animIndices.size() > 0) ? animIndices[0] : 0u,
            /* .animIdx1 = */ (animIndices.size() > 1) ? animIndices[1] : 0u,
            /* .animWeight0 = */ (weights.size() > 0) ? weights[0] : 0.0f,
            /* .animWeight1 = */ (weights.size() > 1) ? weights[1] : 0.0f,
            /* .sampleIdx0 = */ (sampleIndices.size() > 0) ? sampleIndices[0] : 0,
            /* .sampleIdx1 = */ (sampleIndices.size() > 1) ? sampleIndices[1] : 0,
            /* .boneCnt = */ static_cast<std::uint32_t>(boneCntInSkeleton),
            /* .usePresampled = */ clipmodes.size() > 0 ? (clipmodes[0] == AnimInstance::ClipMode::Presampled) : false
        );

        if (pidCache_.size() == shader().maxInstanceCnt()) [[unlikely]] {
            break;
        }
    }


    // auto lightCache_ = std::vector<sr::Light>();
    lightCache_.clear();
    lightCache_.reserve( lights_.size() );

    for (const auto& light : lights_) {
        lightCache_.emplace_back( light->toViewLight(*pCamera_) );

        if (lightCache_.size() == shader().maxLightCnt()) [[unlikely]] {
            break;
        }
    }


    shadowArrayMaterial_.texture().commitState(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


    // temporarilly directional light at first
    const auto pDirectionalLight = lights_.front();

    const auto pfd = sr::PerFrameData2{
        .globalAmbient = dx::XMFLOAT3(0.1f, 0.1f, 0.1f),
		.shadowMapRef = {
            shadowArrayMaterial_.mapRef(Material::MapType::ShadowCascade0).toxm(),
            shadowArrayMaterial_.mapRef(Material::MapType::ShadowCascade1).toxm(),
            shadowArrayMaterial_.mapRef(Material::MapType::ShadowCascade2).toxm(),
		},
		.lightVP = {
			mu::transpose(pDirectionalLight->calcCascadeViewProj(*pCamera_, 0)).getXmf(),
			mu::transpose(pDirectionalLight->calcCascadeViewProj(*pCamera_, 1)).getXmf(),
			mu::transpose(pDirectionalLight->calcCascadeViewProj(*pCamera_, 2)).getXmf()
		},
        .lightCnt = static_cast<std::uint32_t>( lightCache_.size() )
    };

    shader().perInstanceData_.stage(pidCache_.data(), pidCache_.size() * sizeof(sr::PerInstanceData5));
    shader().lightBuffer_.stage(lightCache_.data(), lightCache_.size() * sizeof(sr::Light));
    shader().perFrameData_.stage(&pfd, sizeof(sr::PerFrameData2));
    shader().boneBuffer_.stage(boneCache_.data(), boneCache_.size() * sizeof(dx::XMFLOAT4X4));
}

void PBRAnimatedIllumination::render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    renderTargets.bind(cmdList, RenderTargets::Specifier::Main);

    auto first = batch_.begin();
    auto accDrawcallCnt = 0u;

    // instances that are going to be drawn are at the front of the batch
    // as the first criterion of sorting is the culled status.
    auto proj = [this](const auto& tuple) {
        return std::tuple(
            std::get<bool>(tuple),
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
    };

    while (first != batch_.end()) {
        auto last = std::ranges::upper_bound(first, batch_.end(), proj(*first), std::less<>{}, proj);

        // as the first criterion of sorting is the culled status,
        // if the first instance is culled, then all the rest are culled.
        auto willNotDraw = std::get<bool>(*first);
        if (willNotDraw) {
            break;
        }

        auto pSubmesh = std::get<gfx::d3d12::Submesh*>(*first);

        auto material = sr::PBRMaterial::convert( pSubmesh->material() );

        auto pdd = sr::PerDrawcallData0{
            .material = material,
            .instanceBase = static_cast<std::uint32_t>(first - batch_.begin()),
            .samplerIdx = pSamplerStorage_->get( SamplerStorage::Indices::TrilinearBorder ).index(),
            .shadowSamplerIdx = pSamplerStorage_->get( SamplerStorage::Indices::BilinearComparison ).index()
        };
        shader().perDrawcallData_.stage( &pdd, sizeof(sr::PerDrawcallData0),
            0u, accDrawcallCnt * shader().cbDrawcallDataSize()
        );

        shader().bindPerDrawcallData(accDrawcallCnt++, cmdList);

        shader().draw( cmdList, *pSubmesh, static_cast<std::size_t>(last - first),
            std::get<VBLayoutIdx>(*first)
        );

        if (accDrawcallCnt == shader().maxDrawcallCnt()) {
            break;
        }

        first = last;
    }
}

void PBRAnimatedIllumination::postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    shadowArrayMaterial_.texture().commitState(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);    
}

void PBRAnimatedIllumination::trackModel(Model* pModel, const AnimController* pAnimCon) {
    if (!pModel->markedRenderPasses().empty()) {
        auto it = std::ranges::find(pModel->markedRenderPasses(), renderPassID());
        if (it == pModel->markedRenderPasses().end()) {
            return;
        }
    }

    auto& nodes = pModel->nodes();
    const auto* pCoord = &pModel->root()->coord();

    for (auto& node : nodes) {
        auto xform = node.coord().xform();
        for (auto& mesh : node.meshes()) {
            auto vbLayoutIdx = protocol_.compatibleLayout(*mesh.refMesh());
            if (!vbLayoutIdx) {
                throw std::runtime_error("Incompatible mesh");
            }

            for (auto& submesh : mesh.submeshes()) {
                batch_.emplace_back(false, nullptr, &submesh, pCoord, vbLayoutIdx.value(), xform, pAnimCon);
            }
        }
    }
}

void PBRAnimatedIllumination::trackModel( Model* pModel,
    const AnimController* pAnimCon, const BoundingVolumeNode* pBVNode
) {
    if (!pModel->markedRenderPasses().empty()) {
        auto it = std::ranges::find(pModel->markedRenderPasses(), renderPassID());
        if (it == pModel->markedRenderPasses().end()) {
            return;
        }
    }

    auto& nodes = pModel->nodes();
    const auto* pCoord = &pModel->root()->coord();

    for (auto& node : nodes) {
        auto xform = node.coord().xform();
        for (auto& mesh : node.meshes()) {
            auto vbLayoutIdx = protocol_.compatibleLayout(*mesh.refMesh());
            if (!vbLayoutIdx) {
                throw std::runtime_error("Incompatible mesh");
            }

            for (auto& submesh : mesh.submeshes()) {
                batch_.emplace_back(false, pBVNode, &submesh, pCoord, vbLayoutIdx.value(), xform, pAnimCon);
            }
        }
    }
}

void PBRAnimatedIllumination::eraseModel( Model* pModel ) {
    for (auto it = batch_.begin(); it != batch_.end();) {
        auto pSubmesh = std::get<gfx::d3d12::Submesh*>(*it);
        if (pSubmesh->parent()->parent()->model() == pModel) {
            it = batch_.erase(it);
        }
        else {
            ++it;
        }
    }
}


ShadowMap::ShadowMap( D3D12Device& device,
    ShaderShadowMap& shader, const D3D12_VIEWPORT& vp
) : gfx::d3d12::RenderPass(id),
    shadowMaterial_(),
    viewport_(vp), protocol_( shader.makeProtocol( device,
        RenderProtocol::Desc{ makeDesc() }
    ) ), pLight_(nullptr), batch_(), pidCache_(), pCamera_(nullptr) {
    viewport_.Width *= 8;
    viewport_.Height *= 8;
}

void ShadowMap::initResources(
    const DescriptorCPU* pDsv,
    const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
    const DescriptorGPU* pSrv, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
) {
    checkRequiredTextures();
    shadowMaterial_ = ShadowMaterial(
        getTexture(RenderPassTextures::ShadowMap),
        pDsv, dsvDesc, pSrv, srvDesc
    );
}

RenderProtocol::Desc ShadowMap::makeDesc() {
    return RenderProtocol::Desc {
        .blend = D3D12_BLEND_DESC{
            .AlphaToCoverageEnable = false,
            .IndependentBlendEnable = false,
            .RenderTarget = {
                D3D12_RENDER_TARGET_BLEND_DESC{
                    .BlendEnable = false,
                    .SrcBlend = D3D12_BLEND_ONE,
                    .DestBlend = D3D12_BLEND_ZERO,
                    .BlendOp = D3D12_BLEND_OP_ADD,
                    .SrcBlendAlpha = D3D12_BLEND_ONE,
                    .DestBlendAlpha = D3D12_BLEND_ZERO,
                    .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                    .RenderTargetWriteMask = 0u
                }
            }
        },
        .sampleMask = UINT_MAX,
        .rasterizerState = D3D12_RASTERIZER_DESC{
            .FillMode = D3D12_FILL_MODE_SOLID,
            .CullMode = D3D12_CULL_MODE_BACK,
            .DepthBias = 1000,
            .DepthBiasClamp = 0.0f,
            .SlopeScaledDepthBias = 1.0f,
            .DepthClipEnable = true
        },
        .depthStencilState = D3D12_DEPTH_STENCIL_DESC{
            .DepthEnable = true,
            .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
            .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
            .StencilEnable = false
        },
        .primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .numRenderTargets = 1u,
        .rtvFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
        .dsvFormat = DXGI_FORMAT_D32_FLOAT,
        .sampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
        .nodeMask = 0u
    };
}

void ShadowMap::setViewport(const D3D12_VIEWPORT& vp) {
    viewport_ = vp;
}

void ShadowMap::preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    cmdList.get()->SetPipelineState( protocol_.get().Get() );
    cmdList.get()->RSSetViewports(1u, &viewport_);
    auto scissorRect = D3D12_RECT{ 0, 0, static_cast<LONG>(viewport_.Width), static_cast<LONG>(viewport_.Height) };
    cmdList.get()->RSSetScissorRects(1u, &scissorRect);
    shadowMaterial_.texture().commitState(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);

    // sort batch by bounding volume node and other properties
    // to cull out the same bounding volume nodes
    // that are out of the camera frustum.
    auto proj = [this](const auto& tuple) {
        return std::tuple(
            std::get<const BoundingVolumeNode*>(tuple),
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
    };

    std::ranges::sort(batch_, std::less<>{}, proj);

    // auto pidCache_ = std::vector<sr::PerInstanceData2>();
    pidCache_.clear();
    pidCache_.reserve( shader().maxInstanceCnt() );

    const BoundingVolumeNode* pLastCulledBVNode = nullptr;

    for (auto& [willNotDraw, pBVNode, pSubmesh, pCoord, vbLayoutIdx, xform] : batch_) {
        // update xform
        xform = pSubmesh->parent()->parent()->coord().xform();

        // cull out instances that are out of the camera frustum.
        // enhance performance by eliding collision check of the same bounding volume nodes.
        if (pLastCulledBVNode && pLastCulledBVNode == pBVNode) {
            willNotDraw = true;
            continue;
        }

        // if (pBVNode && !BoundingVolumeNode::collides(
        //     pCamera_->coordRotation().xform(), pCamera_->bvNode(),
        //     pCoord->xform(), *pBVNode
        // )) {
        //     willNotDraw = true;
        //     pLastCulledBVNode = pBVNode;
        //     continue;
        // }
    }

    // finally sort batch by culled status and other properties
    // to separate instances that are visible and invisible.
    auto proj2 = [this](const auto& tuple) {
        return std::tuple(
            std::get<bool>(tuple),
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
    };

    std::ranges::sort(batch_, std::less<>{}, proj2);

    for (const auto& [willNotDraw, pBVNode, pSubmesh, pCoord, vbLayoutIdx, xform] : batch_) {
        // as the first criterion of sorting is the culled status,
        // if the first instance is culled, then all the rest are culled.
        if (willNotDraw) {
            break;
        }

        // upload per instance data
        pidCache_.emplace_back(
            /* .world = */ mu::transpose( xform ).getXmf()
        );

        if (pidCache_.size() == shader().maxInstanceCnt()) [[unlikely]] {
            break;
        }
    }


    const auto pfd = sr::PerFrameData1{
        .lightVP = mu::transpose( pLight_->viewProj(*pCamera_) ).getXmf()
    };

    shader().perInstanceData_.stage(pidCache_.data(), pidCache_.size() * sizeof(sr::PerInstanceData2));
    shader().perFrameData_.stage(&pfd, sizeof(sr::PerFrameData1));
}

void ShadowMap::render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    shadowMaterial_.texture().commitState(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    cmdList.get()->OMSetRenderTargets(0u, nullptr, false, &shadowMaterial_.dsv().cpuHandle());
    auto first = batch_.begin();
    auto accDrawcallCnt = 0u;

    // instances that are going to be drawn are at the front of the batch
    // as the first criterion of sorting is the culled status.
    auto proj = [this](const auto& tuple) {
        return std::tuple(
            std::get<bool>(tuple),
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
    };

    while (first != batch_.end()) {
        auto last = std::ranges::upper_bound(first, batch_.end(), proj(*first), std::less<>{}, proj);

        // as the first criterion of sorting is the culled status,
        // if the first instance is culled, then all the rest are culled.
        auto willNotDraw = std::get<bool>(*first);
        if (willNotDraw) {
            break;
        }

        auto pSubmesh = std::get<gfx::d3d12::Submesh*>(*first);

        auto pdd = sr::PerDrawcallData2{
            .instanceBase = static_cast<std::uint32_t>(first - batch_.begin()),
        };
        shader().perDrawcallData_.stage( &pdd, sizeof(sr::PerDrawcallData2),
            0u, accDrawcallCnt * shader().cbDrawcallDataSize()
        );

        shader().bindPerDrawcallData(accDrawcallCnt++, cmdList);

        shader().draw( cmdList, *pSubmesh, static_cast<std::size_t>(last - first),
            std::get<VBLayoutIdx>(*first)
        );

        if (accDrawcallCnt == shader().maxDrawcallCnt()) {
            break;
        }

        first = last;
    }
}

void ShadowMap::postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {

}

void ShadowMap::trackModel(Model* pModel) {
    if (!pModel->markedRenderPasses().empty()) {
        auto it = std::ranges::find(pModel->markedRenderPasses(), renderPassID());
        if (it == pModel->markedRenderPasses().end()) {
            return;
        }
    }

    auto& nodes = pModel->nodes();
    const auto* pCoord = &pModel->root()->coord();

    for (auto& node : nodes) {
        auto xform = node.coord().xform();
        for (auto& mesh : node.meshes()) {
            auto vbLayoutIdx = protocol_.compatibleLayout(*mesh.refMesh());
            if (!vbLayoutIdx) {
                throw std::runtime_error("Incompatible mesh");
            }

            for (auto& submesh : mesh.submeshes()) {
                batch_.emplace_back(false, nullptr, &submesh, pCoord, vbLayoutIdx.value(), xform);
            }
        }
    }
}

void ShadowMap::trackModel(Model* pModel, const BoundingVolumeNode* pBVNode) {
    if (!pModel->markedRenderPasses().empty()) {
        auto it = std::ranges::find(pModel->markedRenderPasses(), renderPassID());
        if (it == pModel->markedRenderPasses().end()) {
            return;
        }
    }

    auto& nodes = pModel->nodes();
    const auto* pCoord = &pModel->root()->coord();

    for (auto& node : nodes) {
        auto xform = node.coord().xform();
        for (auto& mesh : node.meshes()) {
            auto vbLayoutIdx = protocol_.compatibleLayout(*mesh.refMesh());
            if (!vbLayoutIdx) {
                throw std::runtime_error("Incompatible mesh");
            }

            for (auto& submesh : mesh.submeshes()) {
                batch_.emplace_back(false, pBVNode, &submesh, pCoord, vbLayoutIdx.value(), xform);
            }
        }
    }
}

void ShadowMap::setLight(const WorldLight* pLight) {
    pLight_ = pLight;
}

CascadeShadowMap::CascadeShadowMap(D3D12Device& device,
    ShaderCascadeShadowMap& shader, const D3D12_VIEWPORT& vp
) : gfx::d3d12::RenderPass(id),
    shadowArrayMaterial_(),
    viewport_(vp), protocol_(shader.makeProtocol(device,
        RenderProtocol::Desc{ makeDesc() }
    )), pLight_(nullptr), batch_(), pidCache_(), pCamera_(nullptr) {
    viewport_.Width *= 4u;
    viewport_.Height *= 4u;
}

void CascadeShadowMap::initResources(RenderPassTextureArrays shadowArrayMap, const DescriptorCPU* pDsv, const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc, const DescriptorGPU* pSrv, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc)
{
    checkRequiredTextureArrays();

    shadowArrayMaterial_ = ShadowArrayMaterial(
        getTextureArray(shadowArrayMap),
        pDsv, dsvDesc, pSrv, srvDesc
    );
}

RenderProtocol::Desc CascadeShadowMap::makeDesc()
{
    return RenderProtocol::Desc{
        .blend = D3D12_BLEND_DESC{
            .AlphaToCoverageEnable = false,
            .IndependentBlendEnable = false,
            .RenderTarget = {
                D3D12_RENDER_TARGET_BLEND_DESC{
                    .BlendEnable = false,
                    .SrcBlend = D3D12_BLEND_ONE,
                    .DestBlend = D3D12_BLEND_ZERO,
                    .BlendOp = D3D12_BLEND_OP_ADD,
                    .SrcBlendAlpha = D3D12_BLEND_ONE,
                    .DestBlendAlpha = D3D12_BLEND_ZERO,
                    .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                    .RenderTargetWriteMask = 0u
                }
            }
        },
        .sampleMask = UINT_MAX,
        .rasterizerState = D3D12_RASTERIZER_DESC{
            .FillMode = D3D12_FILL_MODE_SOLID,
            .CullMode = D3D12_CULL_MODE_BACK,
            .DepthBias = 1000,
            .DepthBiasClamp = 0.0f,
            .SlopeScaledDepthBias = 1.0f,
            .DepthClipEnable = true
        },
        .depthStencilState = D3D12_DEPTH_STENCIL_DESC{
            .DepthEnable = true,
            .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
            .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
            .StencilEnable = false
        },
        .primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .numRenderTargets = 1u,
        .rtvFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
        .dsvFormat = DXGI_FORMAT_D32_FLOAT,
        .sampleDesc = DXGI_SAMPLE_DESC{.Count = 1u, .Quality = 0u },
        .nodeMask = 0u
    };
}

void CascadeShadowMap::setViewport(const D3D12_VIEWPORT& vp) {
    viewport_ = vp;
}

void CascadeShadowMap::preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    cmdList.get()->SetPipelineState(protocol_.get().Get());
    cmdList.get()->RSSetViewports(1u, &viewport_);
    auto scissorRect = D3D12_RECT{ 0, 0, static_cast<LONG>(viewport_.Width), static_cast<LONG>(viewport_.Height) };
    cmdList.get()->RSSetScissorRects(1u, &scissorRect);

    shadowArrayMaterial_.texture().commitState(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);    

    // sort batch by bounding volume node and other properties
    // to cull out the same bounding volume nodes
    // that are out of the camera frustum.
    auto proj = [this](const auto& tuple) {
        return std::tuple(
            std::get<const BoundingVolumeNode*>(tuple),
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
        };

    std::ranges::sort(batch_, std::less<>{}, proj);

    // auto pidCache_ = std::vector<sr::PerInstanceData2>();
    pidCache_.clear();
    pidCache_.reserve(shader().maxInstanceCnt());

    const BoundingVolumeNode* pLastCulledBVNode = nullptr;

    for (auto& [willNotDraw, pBVNode, pSubmesh, pCoord, vbLayoutIdx, xform] : batch_) {
        // update xform
        xform = pSubmesh->parent()->parent()->coord().xform();

        // cull out instances that are out of the camera frustum.
        // enhance performance by eliding collision check of the same bounding volume nodes.
        if (pLastCulledBVNode && pLastCulledBVNode == pBVNode) {
            willNotDraw = true;
            continue;
        }

        // if (pBVNode && !BoundingVolumeNode::collides(
        //     pCamera_->coordRotation().xform(), pCamera_->bvNode(),
        //     pCoord->xform(), *pBVNode
        // )) {
        //     willNotDraw = true;
        //     pLastCulledBVNode = pBVNode;
        //     continue;
        // }
    }

    // finally sort batch by culled status and other properties
    // to separate instances that are visible and invisible.
    auto proj2 = [this](const auto& tuple) {
        return std::tuple(
            std::get<bool>(tuple),
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
        };

    std::ranges::sort(batch_, std::less<>{}, proj2);

    for (const auto& [willNotDraw, pBVNode, pSubmesh, pCoord, vbLayoutIdx, xform] : batch_) {
        // as the first criterion of sorting is the culled status,
        // if the first instance is culled, then all the rest are culled.
        if (willNotDraw) {
            break;
        }

        // upload per instance data
        pidCache_.emplace_back(
            /* .world = */ mu::transpose(xform).getXmf()
        );

        if (pidCache_.size() == shader().maxInstanceCnt()) [[unlikely]] {
            break;
        }
    }

    const auto pfd = sr::PerFrameData1{        
        .lightVP = {            
            mu::transpose(pLight_->calcCascadeViewProj(*pCamera_, 0)).getXmf(),
			mu::transpose(pLight_->calcCascadeViewProj(*pCamera_, 1)).getXmf(),
            mu::transpose(pLight_->calcCascadeViewProj(*pCamera_, 2)).getXmf()            
        }
    };

    shader().perInstanceData_.stage(pidCache_.data(), pidCache_.size() * sizeof(sr::PerInstanceData2));
    shader().perFrameData_.stage(&pfd, sizeof(sr::PerFrameData1));
}

void CascadeShadowMap::render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    shadowArrayMaterial_.texture().commitState(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    cmdList.get()->OMSetRenderTargets(0u, nullptr, false, &shadowArrayMaterial_.dsv().cpuHandle());
    auto first = batch_.begin();
    auto accDrawcallCnt = 0u;

    // instances that are going to be drawn are at the front of the batch
    // as the first criterion of sorting is the culled status.
    auto proj = [this](const auto& tuple) {
        return std::tuple(
            std::get<bool>(tuple),
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
        };

    while (first != batch_.end()) {
        auto last = std::ranges::upper_bound(first, batch_.end(), proj(*first), std::less<>{}, proj);

        // as the first criterion of sorting is the culled status,
        // if the first instance is culled, then all the rest are culled.
        auto willNotDraw = std::get<bool>(*first);
        if (willNotDraw) {
            break;
        }

        auto pSubmesh = std::get<gfx::d3d12::Submesh*>(*first);

        auto pdd = sr::PerDrawcallData2{
            .instanceBase = static_cast<std::uint32_t>(first - batch_.begin()),
        };
        shader().perDrawcallData_.stage(&pdd, sizeof(sr::PerDrawcallData2),
            0u, accDrawcallCnt * shader().cbDrawcallDataSize()
        );

        shader().bindPerDrawcallData(accDrawcallCnt++, cmdList);

        shader().draw(cmdList, *pSubmesh, static_cast<std::size_t>(last - first),
            std::get<VBLayoutIdx>(*first)
        );

        if (accDrawcallCnt == shader().maxDrawcallCnt()) {
            break;
        }

        first = last;
    }
}

void CascadeShadowMap::postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
}

void CascadeShadowMap::trackModel(Model* pModel) {
    if (!pModel->markedRenderPasses().empty()) {
        auto it = std::ranges::find(pModel->markedRenderPasses(), renderPassID());
        if (it == pModel->markedRenderPasses().end()) {
            return;
        }
    }

    auto& nodes = pModel->nodes();
    const auto* pCoord = &pModel->root()->coord();

    for (auto& node : nodes) {
        auto xform = node.coord().xform();
        for (auto& mesh : node.meshes()) {
            auto vbLayoutIdx = protocol_.compatibleLayout(*mesh.refMesh());
            if (!vbLayoutIdx) {
                throw std::runtime_error("Incompatible mesh");
            }

            for (auto& submesh : mesh.submeshes()) {
                batch_.emplace_back(false, nullptr, &submesh, pCoord, vbLayoutIdx.value(), xform);
            }
        }
    }
}

void CascadeShadowMap::trackModel(Model* pModel, const BoundingVolumeNode* pBVNode) {
    if (!pModel->markedRenderPasses().empty()) {
        auto it = std::ranges::find(pModel->markedRenderPasses(), renderPassID());
        if (it == pModel->markedRenderPasses().end()) {
            return;
        }
    }

    auto& nodes = pModel->nodes();
    const auto* pCoord = &pModel->root()->coord();

    for (auto& node : nodes) {
        auto xform = node.coord().xform();
        for (auto& mesh : node.meshes()) {
            auto vbLayoutIdx = protocol_.compatibleLayout(*mesh.refMesh());
            if (!vbLayoutIdx) {
                throw std::runtime_error("Incompatible mesh");
            }

            for (auto& submesh : mesh.submeshes()) {
                batch_.emplace_back(false, pBVNode, &submesh, pCoord, vbLayoutIdx.value(), xform);
            }
        }
    }
}

void CascadeShadowMap::eraseModel(Model* pModel) {
    for (auto it = batch_.begin(); it != batch_.end();) {
        auto pSubmesh = std::get<gfx::d3d12::Submesh*>(*it);
        if (pSubmesh->parent()->parent()->model() == pModel) {
            it = batch_.erase(it);
        }
        else {
            ++it;
        }
    }
}

void CascadeShadowMap::setLight(const WorldLight* pLight) {
    pLight_ = pLight;
}

ShadowMapAnimated::ShadowMapAnimated( D3D12Device& device,
    ShaderShadowMapAnimated& shader, const D3D12_VIEWPORT& vp
) : gfx::d3d12::RenderPass(id),
    shadowMaterial_(),
    viewport_(vp), protocol_( shader.makeProtocol( device,
        RenderProtocol::Desc{ makeDesc() }
    ) ), pLight_(nullptr), batch_(), pidCache_(), pCamera_(nullptr) {
    viewport_.Width *= 8;
    viewport_.Height *= 8;
}

void ShadowMapAnimated::initResources(
    RenderPassTextures shadowMap, const DescriptorCPU* pDsv,
    const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
    const DescriptorGPU* pSrv, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
) {
    checkRequiredTextures();
    shadowMaterial_ = ShadowMaterial(
        getTexture(RenderPassTextures::ShadowMap),
        pDsv, dsvDesc, pSrv, srvDesc
    );
}

RenderProtocol::Desc ShadowMapAnimated::makeDesc() {
    return RenderProtocol::Desc {
        .blend = D3D12_BLEND_DESC{
            .AlphaToCoverageEnable = false,
            .IndependentBlendEnable = false,
            .RenderTarget = {
                D3D12_RENDER_TARGET_BLEND_DESC{
                    .BlendEnable = false,
                    .SrcBlend = D3D12_BLEND_ONE,
                    .DestBlend = D3D12_BLEND_ZERO,
                    .BlendOp = D3D12_BLEND_OP_ADD,
                    .SrcBlendAlpha = D3D12_BLEND_ONE,
                    .DestBlendAlpha = D3D12_BLEND_ZERO,
                    .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                    .RenderTargetWriteMask = 0u
                }
            }
        },
        .sampleMask = UINT_MAX,
        .rasterizerState = D3D12_RASTERIZER_DESC{
            .FillMode = D3D12_FILL_MODE_SOLID,
            .CullMode = D3D12_CULL_MODE_BACK,
            .DepthBias = 1000,
            .DepthBiasClamp = 0.0f,
            .SlopeScaledDepthBias = 1.0f,
            .DepthClipEnable = true
        },
        .depthStencilState = D3D12_DEPTH_STENCIL_DESC{
            .DepthEnable = true,
            .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
            .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
            .StencilEnable = false
        },
        .primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .numRenderTargets = 1u,
        .rtvFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
        .dsvFormat = DXGI_FORMAT_D32_FLOAT,
        .sampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
        .nodeMask = 0u
    };
}

void ShadowMapAnimated::setViewport(const D3D12_VIEWPORT& vp) {
    viewport_ = vp;
}

void ShadowMapAnimated::preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    cmdList.get()->SetPipelineState( protocol_.get().Get() );
    cmdList.get()->RSSetViewports(1u, &viewport_);
    auto scissorRect = D3D12_RECT{ 0, 0, static_cast<LONG>(viewport_.Width), static_cast<LONG>(viewport_.Height) };
    cmdList.get()->RSSetScissorRects(1u, &scissorRect);
    shadowMaterial_.texture().commitState(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);

    // sort batch by bounding volume node and other properties
    // to cull out the same bounding volume nodes
    // that are out of the camera frustum.
    auto proj = [this](const auto& tuple) {
        return std::tuple(
            std::get<const BoundingVolumeNode*>(tuple),
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
    };

    std::ranges::sort(batch_, std::less<>{}, proj);

    const BoundingVolumeNode* pLastCulledBVNode = nullptr;

    for (auto& [willNotDraw, pBVNode, pSubmesh, pCoord, vbLayoutIdx, xform, pAnimCon] : batch_) {
        // update xform
        xform = pSubmesh->parent()->parent()->model()->root()->coord().xform();

        // cull out instances that are out of the camera frustum.
        // enhance performance by eliding collision check of the same bounding volume nodes.
        if (pLastCulledBVNode && pLastCulledBVNode == pBVNode) {
            willNotDraw = true;
            continue;
        }

        // if (pBVNode && !BoundingVolumeNode::collides(
        //     pCamera_->coordRotation().xform(), pCamera_->bvNode(),
        //     pCoord->xform(), *pBVNode
        // )) {
        //     willNotDraw = true;
        //     pLastCulledBVNode = pBVNode;
        //     continue;
        // }
    }

    // finally sort batch by culled status and other properties
    // to separate instances that are visible and invisible.
    auto proj2 = [this](const auto& tuple) {
        return std::tuple(
            std::get<bool>(tuple),
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
    };

    std::ranges::sort(batch_, std::less<>{}, proj2);


    // auto pidCache_ = std::vector<sr::PerInstanceData6>();
    pidCache_.clear();
    pidCache_.reserve( shader().maxInstanceCnt() );

    static constexpr std::size_t targetAnimCntExpected = 4u;

    for (const auto& [willNotDraw, pBVNode, pSubmesh, pCoord, vbLayoutIdx, xform, pAnimCon] : batch_) {
        // as the first criterion of sorting is the culled status,
        // if the first instance is culled, then all the rest are culled.
        if (willNotDraw) {
            break;
        }

        auto weights = std::vector<float>();
        weights.reserve( targetAnimCntExpected );
        auto sampleIndices = std::vector<int>();
        sampleIndices.reserve( targetAnimCntExpected );
        auto animIndices = std::vector<std::uint32_t>();
        animIndices.reserve( targetAnimCntExpected );

        std::size_t boneCntInSkeleton = pAnimCon->skeleton().bones().size();

        for (const auto& [key, animInst] : pAnimCon->instances()) {
            weights.push_back(animInst.weight());

            auto bakedXforms = static_cast<Texture*>(animInst.animClip()->customData());
            if (!bakedXforms) {
                throw GFX_EXCEPT( "[Description]: Presampled animation clip has no baked xforms." );
            }

            animIndices.push_back(static_cast<std::uint32_t>(bakedXforms->view(bakedXforms->idxSrv).offset()));
            sampleIndices.push_back( static_cast<int>(
                animInst.elapsed() / animInst.animClip()->sampleInterval()
            ) );
        }

        // upload per instance data
        pidCache_.emplace_back(
            /* .world = */ mu::transpose( xform ).getXmf(),
            /* .animIdx0 = */ (animIndices.size() > 0) ? animIndices[0] : 0u,
            /* .animIdx1 = */ (animIndices.size() > 1) ? animIndices[1] : 0u,
            /* .animWeight0 = */ (weights.size() > 0) ? weights[0] : 0.0f,
            /* .animWeight1 = */ (weights.size() > 1) ? weights[1] : 0.0f,
            /* .sampleIdx0 = */ (sampleIndices.size() > 0) ? sampleIndices[0] : 0,
            /* .sampleIdx1 = */ (sampleIndices.size() > 1) ? sampleIndices[1] : 0,
            /* .boneCnt = */ static_cast<std::uint32_t>(boneCntInSkeleton)
        );

        if (pidCache_.size() == shader().maxInstanceCnt()) [[unlikely]] {
            break;
        }
    }


    const auto pfd = sr::PerFrameData1{
        .lightVP = mu::transpose( pLight_->viewProj(*pCamera_) ).getXmf()
    };

    shader().perInstanceData_.stage(pidCache_.data(), pidCache_.size() * sizeof(sr::PerInstanceData6));
    shader().perFrameData_.stage(&pfd, sizeof(sr::PerFrameData1));
}

void ShadowMapAnimated::render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    shadowMaterial_.texture().commitState(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    cmdList.get()->OMSetRenderTargets(0u, nullptr, false, &shadowMaterial_.dsv().cpuHandle());
    auto first = batch_.begin();
    auto accDrawcallCnt = 0u;

    // instances that are going to be drawn are at the front of the batch
    // as the first criterion of sorting is the culled status.
    auto proj = [this](const auto& tuple) {
        return std::tuple(
            std::get<bool>(tuple),
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
    };

    while (first != batch_.end()) {
        auto last = std::ranges::upper_bound(first, batch_.end(), proj(*first), std::less<>{}, proj);

        // as the first criterion of sorting is the culled status,
        // if the first instance is culled, then all the rest are culled.
        auto willNotDraw = std::get<bool>(*first);
        if (willNotDraw) {
            break;
        }

        auto pSubmesh = std::get<gfx::d3d12::Submesh*>(*first);

        auto pdd = sr::PerDrawcallData2{
            .instanceBase = static_cast<std::uint32_t>(first - batch_.begin()),
        };
        shader().perDrawcallData_.stage( &pdd, sizeof(sr::PerDrawcallData2),
            0u, accDrawcallCnt * shader().cbDrawcallDataSize()
        );

        shader().bindPerDrawcallData(accDrawcallCnt++, cmdList);

        shader().draw( cmdList, *pSubmesh, static_cast<std::size_t>(last - first),
            std::get<VBLayoutIdx>(*first)
        );

        if (accDrawcallCnt == shader().maxDrawcallCnt()) {
            break;
        }

        first = last;
    }
}

void ShadowMapAnimated::postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {

}

void ShadowMapAnimated::trackModel(Model* pModel, const AnimController* pAnimCon) {
    if (!pModel->markedRenderPasses().empty()) {
        auto it = std::ranges::find(pModel->markedRenderPasses(), renderPassID());
        if (it == pModel->markedRenderPasses().end()) {
            return;
        }
    }

    auto& nodes = pModel->nodes();
    const auto* pCoord = &pModel->root()->coord();

    for (auto& node : nodes) {
        auto xform = node.coord().xform();
        for (auto& mesh : node.meshes()) {
            auto vbLayoutIdx = protocol_.compatibleLayout(*mesh.refMesh());
            if (!vbLayoutIdx) {
                throw std::runtime_error("Incompatible mesh");
            }

            for (auto& submesh : mesh.submeshes()) {
                batch_.emplace_back(false, nullptr, &submesh, pCoord, vbLayoutIdx.value(), xform, pAnimCon);
            }
        }
    }
}

void ShadowMapAnimated::trackModel( Model* pModel, const AnimController* pAnimCon,
    const BoundingVolumeNode* pBVNode
) {
    if (!pModel->markedRenderPasses().empty()) {
        auto it = std::ranges::find(pModel->markedRenderPasses(), renderPassID());
        if (it == pModel->markedRenderPasses().end()) {
            return;
        }
    }

    auto& nodes = pModel->nodes();
    const auto* pCoord = &pModel->root()->coord();

    for (auto& node : nodes) {
        auto xform = node.coord().xform();
        for (auto& mesh : node.meshes()) {
            auto vbLayoutIdx = protocol_.compatibleLayout(*mesh.refMesh());
            if (!vbLayoutIdx) {
                throw std::runtime_error("Incompatible mesh");
            }

            for (auto& submesh : mesh.submeshes()) {
                batch_.emplace_back(false, pBVNode, &submesh, pCoord, vbLayoutIdx.value(), xform, pAnimCon);
            }
        }
    }
}

void ShadowMapAnimated::setLight(const WorldLight* pLight) {
    pLight_ = pLight;
}

CascadeShadowMapAnimated::CascadeShadowMapAnimated( D3D12Device& device,
    ShaderCascadeShadowMapAnimated& shader, const D3D12_VIEWPORT& vp
) : gfx::d3d12::RenderPass(id),
    shadowMaterial_(),
    viewport_(vp), protocol_( shader.makeProtocol( device,
        RenderProtocol::Desc{ makeDesc() }
    ) ), pLight_(nullptr), batch_(), pidCache_(), pCamera_(nullptr) {
    viewport_.Width *= 4u;
    viewport_.Height *= 4u;
}

void CascadeShadowMapAnimated::initResources(
    RenderPassTextureArrays shadowMap, 
    const DescriptorCPU* pDsv,
    const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
    const DescriptorGPU* pSrv, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
) {
    checkRequiredTextureArrays();

    shadowMaterial_ = ShadowArrayMaterial(
        getTextureArray(shadowMap),
        pDsv, dsvDesc, pSrv, srvDesc
    );
}

RenderProtocol::Desc CascadeShadowMapAnimated::makeDesc() {
    return RenderProtocol::Desc {
        .blend = D3D12_BLEND_DESC{
            .AlphaToCoverageEnable = false,
            .IndependentBlendEnable = false,
            .RenderTarget = {
                D3D12_RENDER_TARGET_BLEND_DESC{
                    .BlendEnable = false,
                    .SrcBlend = D3D12_BLEND_ONE,
                    .DestBlend = D3D12_BLEND_ZERO,
                    .BlendOp = D3D12_BLEND_OP_ADD,
                    .SrcBlendAlpha = D3D12_BLEND_ONE,
                    .DestBlendAlpha = D3D12_BLEND_ZERO,
                    .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                    .RenderTargetWriteMask = 0u
                }
            }
        },
        .sampleMask = UINT_MAX,
        .rasterizerState = D3D12_RASTERIZER_DESC{
            .FillMode = D3D12_FILL_MODE_SOLID,
            .CullMode = D3D12_CULL_MODE_BACK,
            .DepthBias = 1000,
            .DepthBiasClamp = 0.0f,
            .SlopeScaledDepthBias = 1.0f,
            .DepthClipEnable = true
        },
        .depthStencilState = D3D12_DEPTH_STENCIL_DESC{
            .DepthEnable = true,
            .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
            .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
            .StencilEnable = false
        },
        .primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .numRenderTargets = 1u,
        .rtvFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
        .dsvFormat = DXGI_FORMAT_D32_FLOAT,
        .sampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
        .nodeMask = 0u
    };
}

void CascadeShadowMapAnimated::setViewport(const D3D12_VIEWPORT& vp) {
    viewport_ = vp;
}

void CascadeShadowMapAnimated::preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    cmdList.get()->SetPipelineState( protocol_.get().Get() );
    cmdList.get()->RSSetViewports(1u, &viewport_);
    auto scissorRect = D3D12_RECT{ 0, 0, static_cast<LONG>(viewport_.Width), static_cast<LONG>(viewport_.Height) };
    cmdList.get()->RSSetScissorRects(1u, &scissorRect);
    shadowMaterial_.texture().commitState(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);

    // sort batch by bounding volume node and other properties
    // to cull out the same bounding volume nodes
    // that are out of the camera frustum.
    auto proj = [this](const auto& tuple) {
        return std::tuple(
            std::get<const BoundingVolumeNode*>(tuple),
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
    };

    std::ranges::sort(batch_, std::less<>{}, proj);

    const BoundingVolumeNode* pLastCulledBVNode = nullptr;

    for (auto& [willNotDraw, pBVNode, pSubmesh, pCoord, vbLayoutIdx, xform, pAnimCon] : batch_) {
        // update xform
        xform = pSubmesh->parent()->parent()->model()->root()->coord().xform();

        // cull out instances that are out of the camera frustum.
        // enhance performance by eliding collision check of the same bounding volume nodes.
        if (pLastCulledBVNode && pLastCulledBVNode == pBVNode) {
            willNotDraw = true;
            continue;
        }

        // if (pBVNode && !BoundingVolumeNode::collides(
        //     pCamera_->coordRotation().xform(), pCamera_->bvNode(),
        //     pCoord->xform(), *pBVNode
        // )) {
        //     willNotDraw = true;
        //     pLastCulledBVNode = pBVNode;
        //     continue;
        // }
    }

    // finally sort batch by culled status and other properties
    // to separate instances that are visible and invisible.
    auto proj2 = [this](const auto& tuple) {
        return std::tuple(
            std::get<bool>(tuple),
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
    };

    std::ranges::sort(batch_, std::less<>{}, proj2);


    /// auto pidCache_ = std::vector<sr::PerInstanceData6>();
    pidCache_.clear();
    pidCache_.reserve( shader().maxInstanceCnt() );

    static constexpr std::size_t targetAnimCntExpected = 4u;

    for (const auto& [willNotDraw, pBVNode, pSubmesh, pCoord, vbLayoutIdx, xform, pAnimCon] : batch_) {
        // as the first criterion of sorting is the culled status,
        // if the first instance is culled, then all the rest are culled.
        if (willNotDraw) {
            break;
        }

        auto weights = std::vector<float>();
        weights.reserve( targetAnimCntExpected );
        auto sampleIndices = std::vector<int>();
        sampleIndices.reserve( targetAnimCntExpected );
        auto animIndices = std::vector<std::uint32_t>();
        animIndices.reserve( targetAnimCntExpected );

        std::size_t boneCntInSkeleton = pAnimCon->skeleton().bones().size();

        for (const auto& [key, animInst] : pAnimCon->instances()) {
            weights.push_back(animInst.weight());

            auto bakedXforms = static_cast<Texture*>(animInst.animClip()->customData());
            if (!bakedXforms) {
                throw GFX_EXCEPT( "[Description]: Presampled animation clip has no baked xforms." );
            }

            animIndices.push_back(static_cast<std::uint32_t>(bakedXforms->view(bakedXforms->idxSrv).offset()));
            sampleIndices.push_back( static_cast<int>(
                animInst.elapsed() / animInst.animClip()->sampleInterval()
            ) );
        }

        // upload per instance data
        pidCache_.emplace_back(
            /* .world = */ mu::transpose( xform ).getXmf(),
            /* .animIdx0 = */ (animIndices.size() > 0) ? animIndices[0] : 0u,
            /* .animIdx1 = */ (animIndices.size() > 1) ? animIndices[1] : 0u,
            /* .animWeight0 = */ (weights.size() > 0) ? weights[0] : 0.0f,
            /* .animWeight1 = */ (weights.size() > 1) ? weights[1] : 0.0f,
            /* .sampleIdx0 = */ (sampleIndices.size() > 0) ? sampleIndices[0] : 0,
            /* .sampleIdx1 = */ (sampleIndices.size() > 1) ? sampleIndices[1] : 0,
            /* .boneCnt = */ static_cast<std::uint32_t>(boneCntInSkeleton)
        );

        if (pidCache_.size() == shader().maxInstanceCnt()) [[unlikely]] {
            break;
        }
    }


    const auto pfd = sr::PerFrameData1{
        .lightVP = {
            mu::transpose(pLight_->calcCascadeViewProj(*pCamera_, 0)).getXmf(),
            mu::transpose(pLight_->calcCascadeViewProj(*pCamera_, 1)).getXmf(),
            mu::transpose(pLight_->calcCascadeViewProj(*pCamera_, 2)).getXmf()
        },
    };

    shader().perInstanceData_.stage(pidCache_.data(), pidCache_.size() * sizeof(sr::PerInstanceData6));
    shader().perFrameData_.stage(&pfd, sizeof(sr::PerFrameData1));
}

void CascadeShadowMapAnimated::render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    shadowMaterial_.texture().commitState(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    cmdList.get()->OMSetRenderTargets(0u, nullptr, false, &shadowMaterial_.dsv().cpuHandle());
    auto first = batch_.begin();
    auto accDrawcallCnt = 0u;

    // instances that are going to be drawn are at the front of the batch
    // as the first criterion of sorting is the culled status.
    auto proj = [this](const auto& tuple) {
        return std::tuple(
            std::get<bool>(tuple),
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
    };

    while (first != batch_.end()) {
        auto last = std::ranges::upper_bound(first, batch_.end(), proj(*first), std::less<>{}, proj);

        // as the first criterion of sorting is the culled status,
        // if the first instance is culled, then all the rest are culled.
        auto willNotDraw = std::get<bool>(*first);
        if (willNotDraw) {
            break;
        }

        auto pSubmesh = std::get<gfx::d3d12::Submesh*>(*first);

        auto pdd = sr::PerDrawcallData2{
            .instanceBase = static_cast<std::uint32_t>(first - batch_.begin()),
        };
        shader().perDrawcallData_.stage( &pdd, sizeof(sr::PerDrawcallData2),
            0u, accDrawcallCnt * shader().cbDrawcallDataSize()
        );

        shader().bindPerDrawcallData(accDrawcallCnt++, cmdList);

        shader().draw( cmdList, *pSubmesh, static_cast<std::size_t>(last - first),
            std::get<VBLayoutIdx>(*first)
        );

        if (accDrawcallCnt == shader().maxDrawcallCnt()) {
            break;
        }

        first = last;
    }
}

void CascadeShadowMapAnimated::postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
}

void CascadeShadowMapAnimated::trackModel(Model* pModel, const AnimController* pAnimCon) {
    if (!pModel->markedRenderPasses().empty()) {
        auto it = std::ranges::find(pModel->markedRenderPasses(), renderPassID());
        if (it == pModel->markedRenderPasses().end()) {
            return;
        }
    }

    auto& nodes = pModel->nodes();
    const auto* pCoord = &pModel->root()->coord();

    for (auto& node : nodes) {
        auto xform = node.coord().xform();
        for (auto& mesh : node.meshes()) {
            auto vbLayoutIdx = protocol_.compatibleLayout(*mesh.refMesh());
            if (!vbLayoutIdx) {
                throw std::runtime_error("Incompatible mesh");
            }

            for (auto& submesh : mesh.submeshes()) {
                batch_.emplace_back(false, nullptr, &submesh, pCoord, vbLayoutIdx.value(), xform, pAnimCon);
            }
        }
    }
}

void CascadeShadowMapAnimated::trackModel( Model* pModel, const AnimController* pAnimCon,
    const BoundingVolumeNode* pBVNode
) {
    if (!pModel->markedRenderPasses().empty()) {
        auto it = std::ranges::find(pModel->markedRenderPasses(), renderPassID());
        if (it == pModel->markedRenderPasses().end()) {
            return;
        }
    }

    auto& nodes = pModel->nodes();
    const auto* pCoord = &pModel->root()->coord();

    for (auto& node : nodes) {
        auto xform = node.coord().xform();
        for (auto& mesh : node.meshes()) {
            auto vbLayoutIdx = protocol_.compatibleLayout(*mesh.refMesh());
            if (!vbLayoutIdx) {
                throw std::runtime_error("Incompatible mesh");
            }

            for (auto& submesh : mesh.submeshes()) {
                batch_.emplace_back(false, pBVNode, &submesh, pCoord, vbLayoutIdx.value(), xform, pAnimCon);
            }
        }
    }
}

void CascadeShadowMapAnimated::eraseModel(Model* pModel) {
    for (auto it = batch_.begin(); it != batch_.end();) {
        auto pSubmesh = std::get<gfx::d3d12::Submesh*>(*it);
        if (pSubmesh->parent()->parent()->model() == pModel) {
            it = batch_.erase(it);
        }
        else {
            ++it;
        }
    }
}

void CascadeShadowMapAnimated::setLight(const WorldLight* pLight) {
    pLight_ = pLight;
}

ScreenQuad::ScreenQuad( D3D12Device& device, ShaderScreenQuad& shader,
    const SamplerStorage& samplerStorage, const D3D12_VIEWPORT& vp
) : gfx::d3d12::RenderPass(id),
    viewport_(vp), protocol_( shader.makeProtocol( device,
        RenderProtocol::Desc{ makeDesc() }
    ) ), pSamplerStorage_(&samplerStorage) {}

RenderProtocol::Desc ScreenQuad::makeDesc() {
    return RenderProtocol::Desc {
        .blend = D3D12_BLEND_DESC{
            .AlphaToCoverageEnable = false,
            .IndependentBlendEnable = false,
            .RenderTarget = {
                D3D12_RENDER_TARGET_BLEND_DESC{
                    .BlendEnable = false,
                    .SrcBlend = D3D12_BLEND_ONE,
                    .DestBlend = D3D12_BLEND_ZERO,
                    .BlendOp = D3D12_BLEND_OP_ADD,
                    .SrcBlendAlpha = D3D12_BLEND_ONE,
                    .DestBlendAlpha = D3D12_BLEND_ZERO,
                    .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                    .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
                }
            }
        },
        .sampleMask = UINT_MAX,
        .rasterizerState = D3D12_RASTERIZER_DESC{
            .FillMode = D3D12_FILL_MODE_SOLID,
            .CullMode = D3D12_CULL_MODE_NONE,
            .DepthClipEnable = true
        },
        .depthStencilState = D3D12_DEPTH_STENCIL_DESC{
            .DepthEnable = false,
            .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
            .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
            .StencilEnable = false
        },
        .primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .numRenderTargets = 1u,
        .rtvFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
        .dsvFormat = DXGI_FORMAT_D32_FLOAT,
        .sampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
        .nodeMask = 0u
    };
}

void ScreenQuad::setViewport(const D3D12_VIEWPORT& vp) {
    viewport_ = vp;
}

void ScreenQuad::preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    cmdList.get()->SetPipelineState( protocol_.get().Get() );
    cmdList.get()->RSSetViewports(1u, &viewport_);
    auto scissorRect = D3D12_RECT{ 0, 0, static_cast<LONG>(viewport_.Width), static_cast<LONG>(viewport_.Height) };
    cmdList.get()->RSSetScissorRects(1u, &scissorRect);
}

void ScreenQuad::render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    renderTargets.bind(cmdList, RenderTargets::Specifier::Main);
    renderTargets.clear(cmdList, RenderTargets::Specifier::Main);

    const auto pdd = sr::PerDrawcallData3{
        .frameMapRef = shader().screenQuad_.mapRefArrayIndex().toxm(),
        .samplerIdx = pSamplerStorage_->get( SamplerStorage::Indices::TrilinearBorder).index()
    };

    shader().perDrawcallData_.stage(&pdd, sizeof(sr::PerDrawcallData3));

    shader().draw( cmdList );
}

void ScreenQuad::postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {}

Tessellation::Tessellation( D3D12Device& device,
    ShaderTessellation& shader, const SamplerStorage& samplerStorage,
    const D3D12_VIEWPORT& vp
) : gfx::d3d12::RenderPass(id), shadowArrayMaterial_(),
    viewport_(vp), protocol_( shader.makeProtocol( device,
        RenderProtocol::Desc{ makeDesc() }
    ) ), lights_(), batch_(), pidCache_(), lightCache_(), pCamera_(nullptr),
    pSamplerStorage_(&samplerStorage) {}

void Tessellation::initResources(
    RenderPassTextureArrays shadowArrayMap,
    const DescriptorCPU* pDsv,
    const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
    const DescriptorGPU* pSrv, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
) {
    checkRequiredTextureArrays();
    shadowArrayMaterial_ = ShadowArrayMaterial(
        getTextureArray(shadowArrayMap),
        pDsv, dsvDesc, pSrv, srvDesc
    );
}

RenderProtocol::Desc Tessellation::makeDesc() {
    return RenderProtocol::Desc {
        .blend = D3D12_BLEND_DESC{
            .AlphaToCoverageEnable = false,
            .IndependentBlendEnable = false,
            .RenderTarget = {
                D3D12_RENDER_TARGET_BLEND_DESC{
                    .BlendEnable = false,
                    .SrcBlend = D3D12_BLEND_ONE,
                    .DestBlend = D3D12_BLEND_ZERO,
                    .BlendOp = D3D12_BLEND_OP_ADD,
                    .SrcBlendAlpha = D3D12_BLEND_ONE,
                    .DestBlendAlpha = D3D12_BLEND_ZERO,
                    .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                    .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
                }
            }
        },
        .sampleMask = UINT_MAX,
        .rasterizerState = D3D12_RASTERIZER_DESC{
            .FillMode = D3D12_FILL_MODE_SOLID,
            .CullMode = D3D12_CULL_MODE_BACK,
            .DepthClipEnable = true,
        },
        .depthStencilState = D3D12_DEPTH_STENCIL_DESC{
            .DepthEnable = true,
            .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
            .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
            .StencilEnable = false
        },
        .primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH,
        .numRenderTargets = 1u,
        .rtvFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
        .dsvFormat = DXGI_FORMAT_D32_FLOAT,
        .sampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
        .nodeMask = 0u
    };
}

void Tessellation::setViewport(const D3D12_VIEWPORT& vp) {
    viewport_ = vp;
}

void Tessellation::preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    cmdList.get()->SetPipelineState( protocol_.get().Get() );
    cmdList.get()->RSSetViewports(1u, &viewport_);
    auto scissorRect = D3D12_RECT{ 0, 0, static_cast<LONG>(viewport_.Width), static_cast<LONG>(viewport_.Height) };
    cmdList.get()->RSSetScissorRects(1u, &scissorRect);

    // auto pidCache_ = std::vector<sr::PerInstanceData0>();
    pidCache_.clear();
    pidCache_.reserve( shader().maxInstanceCnt() );

    for (const auto& pChunk : batch_) {
        auto xform = pChunk->idxToWorld();
        pidCache_.emplace_back(
            /* .wvp = */ mu::transpose( xform * pCamera_->view() * pCamera_->proj() ).getXmf(),
            /* .world = */ mu::transpose( xform ).getXmf(),
            /* .wv = */ mu::transpose( xform * pCamera_->view() ).getXmf(),
            /* .wvNormal = */ dx::convertMat<dx::XMFLOAT3X3>(
                mu::inverse(xform * pCamera_->view()).get()
            )
        );

        if (pidCache_.size() == shader().maxInstanceCnt()) [[unlikely]] {
            break;
        }
    }

    //  auto lightCache_ = std::vector<sr::Light>();
    lightCache_.clear();
    lightCache_.reserve( lights_.size() );

    for (const auto& light : lights_) {
        lightCache_.emplace_back( light->toViewLight(*pCamera_) );

        if (lightCache_.size() == shader().maxLightCnt()) [[unlikely]] {
            break;
        }
    }

    // temporarilly directional light at first
    const auto pDirectionalLight = lights_.front();


    shadowArrayMaterial_.texture().commitState(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);


    auto pfd = sr::PerFrameData2{
        .globalAmbient = dx::XMFLOAT3(0.1f, 0.1f, 0.1f),
        .shadowMapRef = {
            shadowArrayMaterial_.mapRef(Material::MapType::ShadowCascade0).toxm(),
            shadowArrayMaterial_.mapRef(Material::MapType::ShadowCascade1).toxm(),
            shadowArrayMaterial_.mapRef(Material::MapType::ShadowCascade2).toxm(),
        },
        .lightVP = {
			mu::transpose(pDirectionalLight->calcCascadeViewProj(*pCamera_, 0)).getXmf(),
			mu::transpose(pDirectionalLight->calcCascadeViewProj(*pCamera_, 1)).getXmf(),
			mu::transpose(pDirectionalLight->calcCascadeViewProj(*pCamera_, 2)).getXmf()
		},
        .lightCnt = static_cast<std::uint32_t>( lightCache_.size() ),
    };

    shader().perInstanceData_.stage(pidCache_.data(), pidCache_.size() * sizeof(sr::PerInstanceData0));
    shader().lightBuffer_.stage(lightCache_.data(), lightCache_.size() * sizeof(sr::Light));
    shader().perFrameData_.stage(&pfd, sizeof(sr::PerFrameData2));
}

void Tessellation::render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    renderTargets.bind(cmdList, RenderTargets::Specifier::Main);

    auto accDrawcallCnt = 0u;
    for (const auto& pChunk : batch_) {
        auto material = sr::PBRMaterial::convert( pChunk->material() );
        auto pdd = sr::PerDrawcallData4{
            .material = material,
            .heightMapRef = pChunk->material().mapRef(Material::MapType::Height).toxm(),
            .instanceBase = accDrawcallCnt,
            .shadowSamplerIdx = pSamplerStorage_->get( SamplerStorage::Indices::BilinearComparison ).index(),
            .heightMapSamplerIdx = pSamplerStorage_->get( SamplerStorage::Indices::TrilinearClamp ).index(),
            .samplerIdx = pSamplerStorage_->get( SamplerStorage::Indices::TrilinearWrap ).index(),
            .tileScale = mu::Vec2(33.f).getXmf()  // TODO: get from material
        };

        shader().perDrawcallData_.stage( &pdd, sizeof(sr::PerDrawcallData4),
            0u, accDrawcallCnt * shader().cbDrawcallDataSize()
        );

        shader().bindPerDrawcallData(accDrawcallCnt++, cmdList);

        shader().draw( cmdList, *pChunk );

        if (accDrawcallCnt == shader().maxDrawcallCnt()) {
            break;
        }
    }
}

void Tessellation::postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    shadowArrayMaterial_.texture().commitState(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);	
}

void Tessellation::trackChunk(const LevelChunkModel* pModel) {
    if (!pModel->markedRenderPasses().empty()) {
        auto it = std::ranges::find(pModel->markedRenderPasses(), renderPassID());
        if (it == pModel->markedRenderPasses().end()) {
            return;
        }
    }

    batch_.push_back(pModel);
}

ShadowMapTessellation::ShadowMapTessellation( D3D12Device& device,
    ShaderShadowMapTessellation& shader, const SamplerStorage& samplerStorage,
    const D3D12_VIEWPORT& vp
) : gfx::d3d12::RenderPass(id),
    viewport_(vp), protocol_( shader.makeProtocol( device,
        RenderProtocol::Desc{ makeDesc() }
    ) ), pLight_(nullptr), batch_(), pidCache_(),
    pCamera_(nullptr), pSamplerStorage_(&samplerStorage) {
    viewport_.Width *= 4u;
    viewport_.Height *= 4u;
}

void ShadowMapTessellation::initResources(
    RenderPassTextureArrays shadowArrayMap,
    const DescriptorCPU* pDsv,
    const D3D12_DEPTH_STENCIL_VIEW_DESC& dsvDesc,
    const DescriptorGPU* pSrv, const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc
) {
    checkRequiredTextureArrays();
    shadowArrayMaterial_ = ShadowArrayMaterial(
        getTextureArray(shadowArrayMap),
        pDsv, dsvDesc, pSrv, srvDesc
    );
}

RenderProtocol::Desc ShadowMapTessellation::makeDesc() {
    return RenderProtocol::Desc {
        .blend = D3D12_BLEND_DESC{
            .AlphaToCoverageEnable = false,
            .IndependentBlendEnable = false,
            .RenderTarget = {
                D3D12_RENDER_TARGET_BLEND_DESC{
                    .BlendEnable = false,
                    .SrcBlend = D3D12_BLEND_ONE,
                    .DestBlend = D3D12_BLEND_ZERO,
                    .BlendOp = D3D12_BLEND_OP_ADD,
                    .SrcBlendAlpha = D3D12_BLEND_ONE,
                    .DestBlendAlpha = D3D12_BLEND_ZERO,
                    .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                    .RenderTargetWriteMask = 0u
                }
            }
        },
        .sampleMask = UINT_MAX,
        .rasterizerState = D3D12_RASTERIZER_DESC{
            .FillMode = D3D12_FILL_MODE_SOLID,
            .CullMode = D3D12_CULL_MODE_BACK,
            .DepthBias = 2000,
            .DepthBiasClamp = 0.0f,
            .SlopeScaledDepthBias = 2.0f,
            .DepthClipEnable = true
        },
        .depthStencilState = D3D12_DEPTH_STENCIL_DESC{
            .DepthEnable = true,
            .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
            .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
            .StencilEnable = false
        },
        .primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH,
        .numRenderTargets = 1u,
        .rtvFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
        .dsvFormat = DXGI_FORMAT_D32_FLOAT,
        .sampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
        .nodeMask = 0u
    };
}

void ShadowMapTessellation::setViewport(const D3D12_VIEWPORT& vp) {
    viewport_ = vp;
}

void ShadowMapTessellation::preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    cmdList.get()->SetPipelineState( protocol_.get().Get() );
    cmdList.get()->RSSetViewports(1u, &viewport_);
    auto scissorRect = D3D12_RECT{ 0, 0, static_cast<LONG>(viewport_.Width), static_cast<LONG>(viewport_.Height) };
    cmdList.get()->RSSetScissorRects(1u, &scissorRect);

    // auto pidCache_ = std::vector<sr::PerInstanceData4>();
    pidCache_.clear();
    pidCache_.reserve( shader().maxInstanceCnt() );

    for (const auto& pChunk : batch_) {
        auto xform = pChunk->idxToWorld();
        pidCache_.emplace_back(
            /* .world = */ mu::transpose( xform ).getXmf(),
            /* .wv = */ mu::transpose( xform * pCamera_->view() ).getXmf()
        );

        if (pidCache_.size() == shader().maxInstanceCnt()) [[unlikely]] {
            break;
        }
    }

    const auto pfd = sr::PerFrameData1{
        .lightVP = {
            mu::transpose(pLight_->calcCascadeViewProj(*pCamera_, 0)).getXmf(),
            mu::transpose(pLight_->calcCascadeViewProj(*pCamera_, 1)).getXmf(),
            mu::transpose(pLight_->calcCascadeViewProj(*pCamera_, 2)).getXmf()
        }
    };

    shader().perInstanceData_.stage(pidCache_.data(), pidCache_.size() * sizeof(sr::PerInstanceData4));
    shader().perFrameData_.stage(&pfd, sizeof(sr::PerFrameData1));
}

void ShadowMapTessellation::render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    shadowArrayMaterial_.texture().commitState(cmdList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    cmdList.get()->OMSetRenderTargets(0u, nullptr, false, &shadowArrayMaterial_.dsv().cpuHandle());

    auto accDrawcallCnt = 0u;
    for (const auto& pChunk : batch_) {
        auto material = sr::PBRMaterial::convert( pChunk->material() );
        auto pdd = sr::PerDrawcallData4{
            .material = material,
            .heightMapRef = pChunk->material().mapRef(Material::MapType::Height).toxm(),
            .instanceBase = accDrawcallCnt,
            .heightMapSamplerIdx = pSamplerStorage_->get( SamplerStorage::Indices::TrilinearClamp ).index(),
            .samplerIdx = pSamplerStorage_->get( SamplerStorage::Indices::TrilinearWrap ).index(),
            .tileScale = mu::Vec2(33.f).getXmf()  // TODO: get from material
        };

        shader().perDrawcallData_.stage( &pdd, sizeof(sr::PerDrawcallData4),
            0u, accDrawcallCnt * shader().cbDrawcallDataSize()
        );

        shader().bindPerDrawcallData(accDrawcallCnt++, cmdList);

        shader().draw( cmdList, *pChunk );

        if (accDrawcallCnt == shader().maxDrawcallCnt()) {
            break;
        }
    }
}

void ShadowMapTessellation::postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
}

void ShadowMapTessellation::trackChunk(const LevelChunkModel* pModel) {
    if (!pModel->markedRenderPasses().empty()) {
        auto it = std::ranges::find(pModel->markedRenderPasses(), renderPassID());
        if (it == pModel->markedRenderPasses().end()) {
            return;
        }
    }

    batch_.push_back(pModel);
}

void ShadowMapTessellation::setLight(const WorldLight* pLight) {
    pLight_ = pLight;
}

Skybox::Skybox(D3D12Device& device, ShaderSkybox& shader,
    const SamplerStorage& samplerStorage, const D3D12_VIEWPORT& vp
) : gfx::d3d12::RenderPass(id),
viewport_(vp),
protocol_(shader.makeProtocol(device,
    RenderProtocol::Desc{ makeDesc() }
)), batch_(), pCamera_(nullptr),
pSamplerStorage_(&samplerStorage) {
}

RenderProtocol::Desc Skybox::makeDesc() {
    return RenderProtocol::Desc{
        .blend = D3D12_BLEND_DESC{
            .AlphaToCoverageEnable = false,
            .IndependentBlendEnable = false,
            .RenderTarget = {
                D3D12_RENDER_TARGET_BLEND_DESC{
                    .BlendEnable = false,
                    .LogicOpEnable = false,
                    .SrcBlend = D3D12_BLEND_ONE,
                    .DestBlend = D3D12_BLEND_ZERO,
                    .BlendOp = D3D12_BLEND_OP_ADD,
                    .SrcBlendAlpha = D3D12_BLEND_ONE,
                    .DestBlendAlpha = D3D12_BLEND_ZERO,
                    .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                    .LogicOp = D3D12_LOGIC_OP_NOOP,
                    .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
                }
            }
        },
        .sampleMask = UINT_MAX,
        .rasterizerState = D3D12_RASTERIZER_DESC{
            .FillMode = D3D12_FILL_MODE_SOLID,
            .CullMode = D3D12_CULL_MODE_NONE,
            .FrontCounterClockwise = false,
            .DepthClipEnable = true
        },
        .depthStencilState = D3D12_DEPTH_STENCIL_DESC{
            .DepthEnable = true,
            .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
            .DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,
            .StencilEnable = false,
            .StencilReadMask = 0xff,
            .StencilWriteMask = 0xff,
            .FrontFace = D3D12_DEPTH_STENCILOP_DESC{
                .StencilFailOp = D3D12_STENCIL_OP_KEEP,
                .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
                .StencilPassOp = D3D12_STENCIL_OP_KEEP,
                .StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS,
            },
            .BackFace = D3D12_DEPTH_STENCILOP_DESC{
                .StencilFailOp = D3D12_STENCIL_OP_KEEP,
                .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
                .StencilPassOp = D3D12_STENCIL_OP_KEEP,
                .StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS,
            }
        },
        .primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .numRenderTargets = 1u,
        .rtvFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
        .dsvFormat = DXGI_FORMAT_D32_FLOAT,
        .sampleDesc = DXGI_SAMPLE_DESC{.Count = 1u, .Quality = 0u },
        .nodeMask = 0u
    };
}

void Skybox::setViewport(const D3D12_VIEWPORT& vp) {
    viewport_ = vp;
}


void Skybox::preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    cmdList.get()->SetPipelineState(protocol_.get().Get());
    cmdList.get()->RSSetViewports(1u, &viewport_);
    auto scissorRect = D3D12_RECT{ 0, 0, static_cast<LONG>(viewport_.Width), static_cast<LONG>(viewport_.Height) };
    cmdList.get()->RSSetScissorRects(1u, &scissorRect);

    // firstly sort batch by bounding volume node and other properties
    // to cull out the same bounding volume nodes
    // that are out of the camera frustum.
    auto proj = [this](const auto& tuple) {
        return std::tuple(
            std::get<const BoundingVolumeNode*>(tuple),
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
        };

    std::ranges::sort(batch_, std::less<>{}, proj);

    const BoundingVolumeNode* pLastCulledBVNode = nullptr;

    for (auto& [willNotDraw, pBVNode, pSubmesh, pCoord, vbLayoutIdx, xform] : batch_) {
        // update xform
        xform = pSubmesh->parent()->parent()->coord().xform();

        // cull out instances that are out of the camera frustum.
        // enhance performance by eliding collision check of the same bounding volume nodes.
        if (pLastCulledBVNode && pLastCulledBVNode == pBVNode) {
            willNotDraw = true;
            continue;
        }

        // if (pBVNode && !BoundingVolumeNode::collides(
        //     pCamera_->coordRotation().xform(), pCamera_->bvNode(),
        //     pCoord->xform(), *pBVNode
        // )) {
        //     willNotDraw = true;
        //     pLastCulledBVNode = pBVNode;
        //     continue;
        // }

        willNotDraw = false;
    }

    // finally sort batch by culled status and other properties
    // to separate instances that are visible and invisible.
    auto proj2 = [this](const auto& tuple) {
        return std::tuple(
            std::get<bool>(tuple),
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
        };

    std::ranges::sort(batch_, std::less<>{}, proj2);

    for (const auto& [willNotDraw, pBVNode, pSubmesh, pCoord, vbLayoutIdx, xform] : batch_) {
        // as the first criterion of sorting is the culled status,
        // if the first instance is culled, then all the rest are culled.
        if (willNotDraw) {
            break;
        }

        // upload per instance data
    }

    const auto pfd = sr::PerFrameData3{
		.vp = mu::transpose(pCamera_->view() * pCamera_->proj()).getXmf(),
    };

    shader().perFrameData_.stage(&pfd, sizeof(sr::PerFrameData3));
}

void Skybox::render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    renderTargets.bind(cmdList, RenderTargets::Specifier::Main);

    auto& refModel = shader().model();

    auto material = refModel.root()->meshes().begin()->submeshes().begin()->material_;

    auto pdd = sr::PerDrawcallData5{
        .material = sr::PBRMaterial::convert(material),
        .samplerIdx = pSamplerStorage_->get(SamplerStorage::Indices::TrilinearWrap).index(),
    };
    shader().perDrawcallData_.stage(&pdd, sizeof(sr::PerDrawcallData5));

    shader().bindPerDrawcallData(0, cmdList);

    shader().draw(cmdList);
}

void Skybox::postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {

}

PlayerUI::PlayerUI(D3D12Device& device, ShaderPlayerUI& shader,
    const SamplerStorage& samplerStorage, const D3D12_VIEWPORT& vp
) : gfx::d3d12::RenderPass(id),
viewport_(vp),
protocol_(shader.makeProtocol(device,
    RenderProtocol::Desc{ makeDesc() }
)), batch_(), pCamera_(nullptr),
pSamplerStorage_(&samplerStorage) {
}

RenderProtocol::Desc PlayerUI::makeDesc() {
    return RenderProtocol::Desc{
        .blend = D3D12_BLEND_DESC{
            .AlphaToCoverageEnable = false,
            .IndependentBlendEnable = false,
            .RenderTarget = {
                D3D12_RENDER_TARGET_BLEND_DESC{
                    .BlendEnable = true,
                    .LogicOpEnable = false,
                    .SrcBlend = D3D12_BLEND_SRC_ALPHA,
                    .DestBlend = D3D12_BLEND_INV_SRC_ALPHA,
                    .BlendOp = D3D12_BLEND_OP_ADD,
                    .SrcBlendAlpha = D3D12_BLEND_ONE,
                    .DestBlendAlpha = D3D12_BLEND_ZERO,
                    .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                    .LogicOp = D3D12_LOGIC_OP_NOOP,
                    .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
                }
            }
        },
        .sampleMask = UINT_MAX,
        .rasterizerState = D3D12_RASTERIZER_DESC{
            .FillMode = D3D12_FILL_MODE_SOLID,
            .CullMode = D3D12_CULL_MODE_NONE,
            .FrontCounterClockwise = false,
            .DepthClipEnable = true
        },
        .depthStencilState = D3D12_DEPTH_STENCIL_DESC{
            .DepthEnable = true,
            .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
            .DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,
            .StencilEnable = false,
            .StencilReadMask = 0xff,
            .StencilWriteMask = 0xff,
            .FrontFace = D3D12_DEPTH_STENCILOP_DESC{
                .StencilFailOp = D3D12_STENCIL_OP_KEEP,
                .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
                .StencilPassOp = D3D12_STENCIL_OP_KEEP,
                .StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS,
            },
            .BackFace = D3D12_DEPTH_STENCILOP_DESC{
                .StencilFailOp = D3D12_STENCIL_OP_KEEP,
                .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
                .StencilPassOp = D3D12_STENCIL_OP_KEEP,
                .StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS,
            }
        },
        .primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .numRenderTargets = 1u,
        .rtvFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
        .dsvFormat = DXGI_FORMAT_D32_FLOAT,
        .sampleDesc = DXGI_SAMPLE_DESC{.Count = 1u, .Quality = 0u },
        .nodeMask = 0u
    };
}

void PlayerUI::setViewport(const D3D12_VIEWPORT& vp) {
    viewport_ = vp;
}


void PlayerUI::preRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    cmdList.get()->SetPipelineState(protocol_.get().Get());
    cmdList.get()->RSSetViewports(1u, &viewport_);
    auto scissorRect = D3D12_RECT{ 0, 0, static_cast<LONG>(viewport_.Width), static_cast<LONG>(viewport_.Height) };
    cmdList.get()->RSSetScissorRects(1u, &scissorRect);

    // firstly sort batch by bounding volume node and other properties
    // to cull out the same bounding volume nodes
    // that are out of the camera frustum.
    auto proj = [this](const auto& tuple) {
        return std::tuple(
            std::get<const BoundingVolumeNode*>(tuple),
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
        };

    std::ranges::sort(batch_, std::less<>{}, proj);

    const BoundingVolumeNode* pLastCulledBVNode = nullptr;

    for (auto& [willNotDraw, pBVNode, pSubmesh, pCoord, vbLayoutIdx, xform] : batch_) {
        // update xform
        xform = pSubmesh->parent()->parent()->coord().xform();

        // cull out instances that are out of the camera frustum.
        // enhance performance by eliding collision check of the same bounding volume nodes.
        if (pLastCulledBVNode && pLastCulledBVNode == pBVNode) {
            willNotDraw = true;
            continue;
        }

        // if (pBVNode && !BoundingVolumeNode::collides(
        //     pCamera_->coordRotation().xform(), pCamera_->bvNode(),
        //     pCoord->xform(), *pBVNode
        // )) {
        //     willNotDraw = true;
        //     pLastCulledBVNode = pBVNode;
        //     continue;
        // }

        willNotDraw = false;
    }

    // finally sort batch by culled status and other properties
    // to separate instances that are visible and invisible.
    auto proj2 = [this](const auto& tuple) {
        return std::tuple(
            std::get<bool>(tuple),
            &std::get<gfx::d3d12::Submesh*>(tuple)->material(),
            std::get<gfx::d3d12::Submesh*>(tuple)->refSubmesh()
        );
        };

    std::ranges::sort(batch_, std::less<>{}, proj2);

    auto pids = std::vector<sr::PerInstanceData2>();
    pids.reserve(shader().maxInstanceCnt());

    for (const auto& [willNotDraw, pBVNode, pSubmesh, pCoord, vbLayoutIdx, xform] : batch_) {
        // as the first criterion of sorting is the culled status,
        // if the first instance is culled, then all the rest are culled.
        if (willNotDraw) {
            break;
        }

        // upload per instance data
                // upload per instance data
        pids.emplace_back(
            /* .world = */ mu::transpose(xform).getXmf()
        );

        if (pids.size() == shader().maxInstanceCnt()) [[unlikely]] {
            break;
        }
    }

    shader().perInstanceData_.stage(pids.data(), pids.size() * sizeof(sr::PerInstanceData2));
}

void PlayerUI::render(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {
    renderTargets.bind(cmdList, RenderTargets::Specifier::Main);

    for (int i = 0; i < 2; ++i)
    {
        auto& refModel = shader().model(i);

        auto material = refModel.root()->meshes().begin()->submeshes().begin()->material_;

        auto pdd = sr::PerDrawcallData6{
            .material = sr::PBRMaterial::convert(material),
            .samplerIdx = pSamplerStorage_->get(SamplerStorage::Indices::TrilinearWrap).index(),
            .instanceBase = static_cast<u32t>(i)
        };
        shader().perDrawcallData_.stage(&pdd, sizeof(sr::PerDrawcallData6));

        shader().bindPerDrawcallData(static_cast<std::size_t>(i), cmdList);

        shader().draw(cmdList, i);
    }
}

void PlayerUI::postRender(D3D12GfxCmdList& cmdList, RenderTargets& renderTargets) {

}

}   // namespace gfx::d3d12::rp

}   // namespace gfx::d3d12

}   // namespace gfx


