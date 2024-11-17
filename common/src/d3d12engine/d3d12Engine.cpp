#include "d3d12engine/d3d12Engine.hpp"

namespace gfx {

namespace d3d12engine {

Core::Core(dx::DXGIFactory& factory)
    : device_( d3d12::getAvailableAdapter(factory, D3D_FEATURE_LEVEL_12_1), D3D_FEATURE_LEVEL_12_1 ),
    cmdQueue_( device_ ), cmdList_( device_ ),
    rtvHeap_(device_, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, initialRtvHeapSize),
    dsvHeap_(device_, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, initialDsvHeapSize),
    cbvSrvUavHeap_(device_, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, initialCbvSrvUavHeapSize),
    descRanges_(rtvHeap_, dsvHeap_, cbvSrvUavHeap_),
    window_(), fence_(device_) {
    window_.open( factory, device_, cmdQueue_, "Project-W",
        Win32::WndFrame{ .x = 100, .y = 100, .width = 1024, .height = 768 },
        descRanges_.rtvRangeBackBuf, descRanges_.dsvRangeBackBuf
    );
    window_.show(SW_SHOW);
}

void Core::render(IRenderer& renderer) {
    cmdList_.reset();
    renderer.render(*this);
    cmdList_.close();
    cmdQueue_.execute(cmdList_);
    window_.present(cmdList_);
    fence_.signal(cmdQueue_);
    fence_.wait();
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