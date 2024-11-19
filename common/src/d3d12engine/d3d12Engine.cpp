#include "d3d12engine/d3d12Engine.hpp"

namespace gfx {

namespace d3d12engine {

Core::Core()
    : staticTexStorage_(), factory_(),
    device_( d3d12::getAvailableAdapter(factory_, D3D_FEATURE_LEVEL_12_1), D3D_FEATURE_LEVEL_12_1 ),
    cmdQueue_( device_ ), cmdList_( device_ ),
    rtvHeap_(device_, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, initialRtvHeapSize),
    dsvHeap_(device_, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, initialDsvHeapSize),
    cbvSrvUavHeap_(device_, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, initialCbvSrvUavHeapSize),
    descRanges_(rtvHeap_, dsvHeap_, cbvSrvUavHeap_),
    root_(device_), window_(), fence_(device_) {
    window_.open( factory_, device_, cmdQueue_, "Project-W",
        Win32::WndFrame{ .x = 100, .y = 100, .width = 1024, .height = 768 },
        descRanges_.rtvRangeBackBuf, descRanges_.dsvRangeBackBuf
    );
    window_.show(SW_SHOW);
    factory_.get().Reset();

    ecs::init( ecs::InitDesc{ .threadCnt = 1u, .entityPoolSize = 0x100u } );
}

void Core::render(IRenderer& renderer) {
    cmdList_.reset();
    cmdList_.get()->SetGraphicsRootSignature(root_.get().Get());
    cmdList_.get()->SetComputeRootSignature(root_.get().Get());
    window_.setRenderTarget(cmdList_);
    window_.clearRenderTarget(cmdList_);
    window_.clearDepthStencil(cmdList_);
    renderer.render(*this);
    window_.setPresent(cmdList_);
    cmdList_.close();
    cmdQueue_.execute(cmdList_);
    window_.present(cmdList_);
    fence_.signal(cmdQueue_);
    fence_.wait();
}

void Core::render() {
    cmdList_.reset();
    cmdList_.get()->SetGraphicsRootSignature(root_.get().Get());
    cmdList_.get()->SetComputeRootSignature(root_.get().Get());
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

void Core::loadStaticTexture( const std::filesystem::path& path,
    d3d12::TextureResource::Type type
) {
    switch (type) {
    case d3d12::TextureResource::Type::Texture:
        staticTexStorage_.load(path, type, device_, cmdList_, descRanges_.srvRangeTex2D);
        break;

    case d3d12::TextureResource::Type::TextureArray:
        staticTexStorage_.load(path, type, device_, cmdList_, descRanges_.srvRangeTex2DArray);
        break;

    case d3d12::TextureResource::Type::TextureCube:
        staticTexStorage_.load(path, type, device_, cmdList_, descRanges_.srvRangeTexCube);
        break;

    default:
        throw GFX_EXCEPT("[Description] Unknown texture type");
    }
}

void CoordRoot::addEntity(ecs::Entity& entity) {
    ecs::System<Coord>::addEntity(entity);
    auto pCoord = entity.get<Coord>();
    if (!pCoord) {
        throw ECS_EXCEPT("Entity does not have a Coord component");
    }
    
    pCoord->get().setParent(&rootCoordSys_);
}

void Scene::addEntity(ecs::Entity& entity) {
    MyBase::addEntity(entity);
    reservedEntities_.push_back(entity.id().value());
}

void Scene::clearStash() {
    reservedEntities_.clear();
}

namespace rp {

void PBRIllumination::init(Scene& scene) {
    for (auto& pModel : models(scene)) {
        trackModel(&pModel->get());
    }
    setCamera(&cameras(scene).front()->get());
}

void PBRIllumination::update(Scene& scene) {
    for (auto& entityID : reservedEntities(scene)) {
        if ( auto pModel = Model::at(entityID) ) {
            trackModel(&pModel->get());
        }
        if ( auto pCamera = Camera::at(entityID) ) {
            setCamera(&pCamera->get());
        }
    }
}

}   // namespace gfx::d3d12engine::rp

}   // namespace gfx::d3d12engine

}   // namespace gfx