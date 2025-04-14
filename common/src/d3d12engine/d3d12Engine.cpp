#include "d3d12engine/d3d12Engine.hpp"

#include <fstream>

#include "resourcePath.hpp"

namespace gfx {

namespace d3d12engine {

Core::Core()
    : samStorage_(), factory_(),
    device_( d3d12::getAvailableAdapter(factory_, D3D_FEATURE_LEVEL_12_1), D3D_FEATURE_LEVEL_12_1 ),
    cmdQueue_( device_ ), cmdList_( device_ ),
    rtvHeap_(device_, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, initialRtvHeapSize),
    dsvHeap_(device_, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, initialDsvHeapSize),
    samHeap_(device_, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, initialSamHeapSize),
    cbvSrvUavHeap_(device_, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, initialCbvSrvUavHeapSize),
    descRanges_(rtvHeap_, dsvHeap_, samHeap_, cbvSrvUavHeap_),
    window_(), fence_(device_) {
    d3d12::UnifiedRoot::init(device_);
    samStorage_.init(device_, descRanges_.samRange, descRanges_.samCmpRange);
    window_.open( factory_, device_, cmdQueue_, "Project-W",
        Win32::WndFrame{ .x = 100, .y = 100, .width = 1600, .height = 900 },
        descRanges_.rtvRangeBackBuf, descRanges_.dsvRangeBackBuf
    );
    window_.show(SW_SHOW);
    factory_.get().Reset();

    ecs::init( ecs::InitDesc{ .threadCnt = 1u, .entityPoolSize = 0x3200u } );
}

void Core::render(IRenderer& renderer, Scene& scene) {
    cmdList_.reset();
    const auto unifiedRoot = d3d12::UnifiedRoot::get();
    cmdList_.get()->SetGraphicsRootSignature(unifiedRoot.get().Get());
    cmdList_.get()->SetComputeRootSignature(unifiedRoot.get().Get());
    d3d12::DescriptorHeapGPU::set(cmdList_.get().Get(), cbvSrvUavHeap_, samHeap_);
    descRanges_.samRange.bind( cmdList_, unifiedRoot.params[
        d3d12::UnifiedRoot::ParamIndices::BindlessSampler
    ]);
    descRanges_.samCmpRange.bind( cmdList_, unifiedRoot.params[
        d3d12::UnifiedRoot::ParamIndices::BindlessSamplerComparison
    ]);
    descRanges_.srvRangeTex2D.bind( cmdList_, unifiedRoot.params[
        d3d12::UnifiedRoot::ParamIndices::BindlessTex2D
    ] );
    descRanges_.srvRangeTex2DArray.bind( cmdList_, unifiedRoot.params[
        d3d12::UnifiedRoot::ParamIndices::BindlessTexArray
    ] );
    descRanges_.srvRangeTexCube.bind( cmdList_, unifiedRoot.params[
        d3d12::UnifiedRoot::ParamIndices::BindlessTexCube
    ] );

    auto renderTargets = d3d12::RenderTargets();
    auto mainRT = d3d12::MainRenderTarget(window_);
    renderTargets.pushTarget(cmdList_, d3d12::RenderTargets::Specifier::Main, &mainRT);
    renderTargets.bind(cmdList_, d3d12::RenderTargets::Specifier::Main);
    renderTargets.clear(cmdList_, d3d12::RenderTargets::Specifier::Main);

    renderer.render(*this, scene, renderTargets);
    
    renderTargets.popTarget(cmdList_, d3d12::RenderTargets::Specifier::Main);
    cmdList_.close();
    cmdQueue_.execute(cmdList_);
    window_.present(cmdList_);
    fence_.signal(cmdQueue_);
    fence_.wait();
}

void Core::render() {
    cmdList_.reset();
    cmdList_.get()->SetGraphicsRootSignature(d3d12::UnifiedRoot::get().get().Get());
    cmdList_.get()->SetComputeRootSignature(d3d12::UnifiedRoot::get().get().Get());
    d3d12::DescriptorHeapGPU::set(cmdList_.get().Get(), cbvSrvUavHeap_, samHeap_);
    window_.setRenderTarget(cmdList_);
    window_.clearRenderTarget(cmdList_);
    window_.clearDepthStencil(cmdList_);
    window_.setPresent(cmdList_);
    cmdList_.close();
    cmdQueue_.execute(cmdList_);
    window_.present(cmdList_);
    fence_.signal(cmdQueue_);
    fence_.wait();
}

Model::Model( const ecs::Entity& entity,
    const d3d12::ResourceStorage::Slot& modelSlot,
    const d3d12::ResourceStorage::ResID& modelKey,
    gameEngine::Coord& coordComp
) : ecs::Component(entity), model_(*modelSlot.get<d3d12::RefModel>(modelKey)) {
    model_.root()->coord().setParent(&coordComp.get());
}

Model::Model( const ecs::Entity& entity,
    const d3d12::RefModel& refModel, gameEngine::Coord& coordComp
) : ecs::Component(entity), model_(refModel) {
    model_.root()->coord().setParent(&coordComp.get());
}

Camera::Camera(const ecs::Entity& entity)
    : ecs::Component(entity), camera_(),
    pAttachedMovement_(nullptr), pAttachedRotation_(nullptr),
    offset_(), timeLag_(0.f) {}

Camera::Camera( const ecs::Entity& entity,
    const d3d12::Camera::Config& config
) : ecs::Component(entity), camera_(config),
    pAttachedMovement_(nullptr), pAttachedRotation_(nullptr),
    offset_(), timeLag_(0.f) {}

void Camera::update(float deltaTime) {
    // following camera
    if (pAttachedMovement_ && pAttachedRotation_) {
        auto offsetRotation = pAttachedRotation_->localXform();
        offsetRotation.setRow(0, mu::Vec4(mu::NVec3(mu::Vec3(offsetRotation.row(0))), 0.f));
        offsetRotation.setRow(1, mu::Vec4(mu::NVec3(mu::Vec3(offsetRotation.row(1))), 0.f));
        offsetRotation.setRow(2, mu::Vec4(mu::NVec3(mu::Vec3(offsetRotation.row(2))), 0.f));
        // temporary
        // offsetRotation *= mu::rotateY(mu::Degree(90.f));

        const auto targetPos = mu::Vec3(pAttachedMovement_->xform().row(3));
        const auto idealPos = targetPos + mu::Vec3( mu::Vec4(offset_, 0.f) * offsetRotation );
        const auto curPos = mu::Vec3(camera_.coordMovement().xform().row(3));
        const auto deltaPos = idealPos - curPos;

        const auto movement = timeLag_ ? deltaPos * deltaTime / timeLag_ : deltaPos * deltaTime;
        camera_.coordMovement() << mu::translate(movement);
        const auto updatedPos = curPos + movement;
        const auto augmentedLook = mu::normalize(targetPos - updatedPos);
        const auto cameraLook = mu::normalize(targetPos - curPos);

        if (movement.len2() > 0.f) {
            auto up = mu::Vec3(0.f, 1.f, 0.f);

            static constexpr auto endurance = 0.000001f;
            if (std::abs(mu::dot(cameraLook, augmentedLook) - 1.0f) >= endurance) {
                const auto rotAxis = mu::cross(cameraLook, augmentedLook);

                const auto rotAngle = mu::acos(mu::dot(cameraLook, augmentedLook));
                up *= mu::rotate(rotAngle, rotAxis);
            }

            // view transform's rotation part is inverse matrix of the camera's rotation matrix
            camera_.coordRotation().setLocalXform(
                mu::transpose( mu::lookAt(mu::Vec3(), mu::Vec3(augmentedLook), mu::Vec3(0.f, 1.f, 0.f)) )
            );
        }
    }
}

void Camera::attach(const Model& model) NOEXCEPT {
    attach(*model.get().root()->coord().parent(), model.get().root()->coord());
}

void Camera::attach(const coord::System& movement, const coord::System& rotation) NOEXCEPT {
    pAttachedMovement_ = &movement;
    pAttachedRotation_ = &rotation;
}

void Camera::detach() NOEXCEPT {
    pAttachedMovement_ = nullptr;
    pAttachedRotation_ = nullptr;
}

void Scene::addEntity(ecs::Entity& entity) {
    MyBase::addEntity(entity);
    reservedEntities_.push_back(entity.id().value());
}

void Scene::clearStash() {
    reservedEntities_.clear();
}

LevelRegion::LevelRegion( const d3d12::ResourceStorage::Slot& heightmapSlot,
    const std::filesystem::path& levelPath,
    const std::filesystem::path& levelTerrainPath
) : pTerrainStream_( std::make_unique<std::ifstream>(levelTerrainPath, std::ios::binary) ),
    pObjectStream_( std::make_unique<std::ifstream>(levelPath, std::ios::binary) ),
    model_(heightmapSlot, *pTerrainStream_),
    dispositionRoot_(*pObjectStream_), chunks_() {}

void LevelRegion::activateChunk(std::size_t xIdx, std::size_t zIdx, Scene& scene) {
    auto& chunk = model_.get(
        dx::XMUINT2(static_cast<std::uint32_t>(xIdx), static_cast<std::uint32_t>(zIdx))
    );

    chunks_.emplace_back().embed(&chunk);
    scene.addEntity(chunks_.back());
}

void LevelRegion::activateChunk( std::size_t xIdx, std::size_t zIdx,
    const d3d12::ResourceStorage::Slot& bvhPathSlot,
    const d3d12::ResourceStorage::ResID& bvhPathKey, Scene& scene,
    CollisionSystem& collisionSystem
) {
    if (!bvhPathSlot.contains<std::filesystem::path>(bvhPathKey)) {
        throw GFX_EXCEPT("[Description] BVH path not registered for key: " + bvhPathKey);
    }

    auto& bvhPath = *bvhPathSlot.get<std::filesystem::path>(bvhPathKey);
    if (!std::filesystem::exists(bvhPath)) {
        throw GFX_EXCEPT("[Description] BVH path not found: " + bvhPath.string());
    }

    activateChunk(xIdx, zIdx, scene);
    chunks_.back().createComponent<BoundingVolume>(bvhPath);
    collisionSystem.addEntity(chunks_.back());
}

std::vector<ecs::Entity> LevelRegion::instantiateAllObjects(
    const d3d12::ResourceStorage::Slot& refModelSlot, coord::System& coordRoot
) {
    auto ret = std::vector<ecs::Entity>();

    instantiateObjectHierarchy(std::nullopt, dispositionRoot_, refModelSlot, coordRoot, ret);

    return ret;
}

void LevelRegion::instantiateObjectHierarchy( std::optional<std::size_t> parentIdx,
    const gameEngine::ObjectDisposition& disposition,
    const d3d12::ResourceStorage::Slot& refModelSlot,
    coord::System& coordRoot, std::vector<ecs::Entity>& out
) {
    if (disposition.prefabName_.empty()) {
        for (auto& child : disposition.children_) {
            instantiateObjectHierarchy(parentIdx, child, refModelSlot, coordRoot, out);
        }
        return;
    }

    const auto modelKey = disposition.prefabName_.substr(2);

    auto obj = ecs::Entity();

    if (!refModelSlot.contains<d3d12::RefModel>(modelKey)) {
        throw GFX_EXCEPT("RefModel not found: " + modelKey);
    }

    obj.createComponent<gameEngine::Coord>();
    obj.as<gameEngine::Coord>().get().setLocalXform(disposition.xform_);
    obj.as<gameEngine::Coord>().get() << mu::translate(0.f, -25.f, 0.f);

    auto& refModel = *refModelSlot.get<d3d12::RefModel>(modelKey);
    obj.createComponent<Model>(refModelSlot, modelKey, obj.as<gameEngine::Coord>());

    if (parentIdx.has_value()) {
        obj.as<gameEngine::Coord>().get().setParent(&out[parentIdx.value()].as<gameEngine::Coord>().get());
    }
    else {
        obj.as<gameEngine::Coord>().get().setParent(&coordRoot);
    }

    out.push_back(std::move(obj));
    std::size_t myIdx = out.size() - 1;

    for (auto& child : disposition.children_) {
        instantiateObjectHierarchy(myIdx, child, refModelSlot, coordRoot, out);
    }
}

namespace rp {

void PBRIllumination::init(Scene& scene) {
    for (auto& pModel : models(scene)) {
        const auto entityID = pModel->entityID().value();
        if (auto pBV = BoundingVolume::atC(entityID)) {
            trackModel(&pModel->get(), &pBV->root());
        }
        else {
            trackModel(&pModel->get());
        }
    }
    if (!cameras(scene).empty()) {
        setCamera(&cameras(scene).front()->get());
    }
    for (auto& pLight : lights(scene)) {
        addLight(&pLight->get());
    }
}

void PBRIllumination::update(Scene& scene) {
    for (auto& entityID : reservedEntities(scene)) {
        if ( auto pModel = Model::at(entityID) ) {
            if (auto pBV = BoundingVolume::atC(entityID)) {
                trackModel(&pModel->get(), &pBV->root());
            }
            else {
                trackModel(&pModel->get());
            }
        }
        if ( auto pCamera = Camera::at(entityID) ) {
            setCamera(&pCamera->get());
        }
        if ( auto pLight = Light::at(entityID) ) {
            addLight(&pLight->get());
        }
    }
}

void PBRAnimatedIllumination::init(Scene& scene) {
    for (auto& pModel : models(scene)) {
        const auto entityID = pModel->entityID().value();

        auto pAnimCon = AnimController::atC(entityID);
        if (!pAnimCon) {
            throw GFX_EXCEPT("[Description] PBRAnimatedIllumination::init: "
                "AnimController not found for entity: " + std::to_string(entityID));
        }

        if (auto pBV = BoundingVolume::atC(entityID)) {
            trackModel(&pModel->get(), pAnimCon, &pBV->root());
        }
        else {
            trackModel(&pModel->get(), pAnimCon);
        }
    }
    if (!cameras(scene).empty()) {
        setCamera(&cameras(scene).front()->get());
    }
    for (auto& pLight : lights(scene)) {
        addLight(&pLight->get());
    }
}

void PBRAnimatedIllumination::update(Scene& scene) {
    for (auto& entityID : reservedEntities(scene)) {
        if ( auto pModel = Model::at(entityID) ) {
            auto pAnimCon = AnimController::atC(entityID);
            if (!pAnimCon) {
                throw GFX_EXCEPT("[Description] PBRAnimatedIllumination::init: "
                    "AnimController not found for entity: " + std::to_string(entityID));
            }

            if (auto pBV = BoundingVolume::atC(entityID)) {
                trackModel(&pModel->get(), pAnimCon, &pBV->root());
            }
            else {
                trackModel(&pModel->get(), pAnimCon);
            }
        }
        if ( auto pCamera = Camera::at(entityID) ) {
            setCamera(&pCamera->get());
        }
        if ( auto pLight = Light::at(entityID) ) {
            addLight(&pLight->get());
        }
    }
}
void ShadowMap::init(Scene& scene) {
    for (auto& pModel : models(scene)) {
        const auto entityID = pModel->entityID().value();
        if (auto pBV = BoundingVolume::atC(entityID)) {
            trackModel(&pModel->get(), &pBV->root());
        }
        else {
            trackModel(&pModel->get());
        }
    }
    if (!cameras(scene).empty()) {
        auto& pCamera = cameras(scene).front();
        if (pCamera) {
            setCamera(&cameras(scene).front()->get());
        }
    }
    if (lights(scene).empty()) {
        throw GFX_EXCEPT("No light found");
    }
    setLight( &lights(scene).front()->get() );
}

void ShadowMap::update(Scene& scene) {
    for (auto& entityID : reservedEntities(scene)) {
        if ( auto pModel = Model::at(entityID) ) {
            if (auto pBV = BoundingVolume::atC(entityID)) {
                trackModel(&pModel->get(), &pBV->root());
            }
            else {
                trackModel(&pModel->get());
            }
        }
        if ( auto pCamera = Camera::at(entityID) ) {
            setCamera(&pCamera->get());
        }
    }
}

void ScreenQuad::init(Scene& scene) {}

void ScreenQuad::update(Scene& scene) {}

void Tessellation::init(Scene& scene) {
    // for (auto& pModel : models(scene)) {
    //    do nothing
    // }
    for (auto& pChunk : levelChunkModels(scene)) {
        trackChunk(&pChunk->get());
    }
    if (!cameras(scene).empty()) {
        auto& pCamera = cameras(scene).front();
        if (pCamera) {
            setCamera(&cameras(scene).front()->get());
        }
    }
    for (auto& pLight : lights(scene)) {
        if (pLight) {
            addLight(&pLight->get());
        }
    }
}

void Tessellation::update(Scene& scene) {
    for (auto& entityID : reservedEntities(scene)) {
        // if ( auto pModel = Model::at(entityID) ) {
        //     do nothing
        // }
        for (auto& pChunk : levelChunkModels(scene)) {
            trackChunk(&pChunk->get());
        }
        if ( auto pCamera = Camera::at(entityID) ) {
            setCamera(&pCamera->get());
        }
        if ( auto pLight = Light::at(entityID) ) {
            addLight(&pLight->get());
        }
    }
}

void ShadowMapTessellation::init(Scene& scene) {
    // for (auto& pModel : models(scene)) {
    //    do nothing
    // }
    for (auto& pChunk : levelChunkModels(scene)) {
        trackChunk(&pChunk->get());
    }
    if (!cameras(scene).empty()) {
        auto& pCamera = cameras(scene).front();
        if (pCamera) {
            setCamera(&cameras(scene).front()->get());
        }
    }
    if (lights(scene).empty()) {
        throw GFX_EXCEPT("No light found");
    }
    setLight( &lights(scene).front()->get() );
}

void ShadowMapTessellation::update(Scene& scene) {
    for (auto& entityID : reservedEntities(scene)) {
        // if ( auto pModel = Model::at(entityID) ) {
        //     do nothing
        // }
        for (auto& pChunk : levelChunkModels(scene)) {
            trackChunk(&pChunk->get());
        }
        if ( auto pCamera = Camera::at(entityID) ) {
            setCamera(&pCamera->get());
        }
    }
}

}   // namespace gfx::d3d12engine::rp

}   // namespace gfx::d3d12engine

}   // namespace gfx