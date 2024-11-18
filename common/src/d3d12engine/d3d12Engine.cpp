#include "d3d12engine/d3d12Engine.hpp"

namespace gfx {

namespace d3d12engine {

Core::Core()
    : factory_(),
    device_( d3d12::getAvailableAdapter(factory_, D3D_FEATURE_LEVEL_12_1), D3D_FEATURE_LEVEL_12_1 ),
    cmdQueue_( device_ ), cmdList_( device_ ),
    rtvHeap_(device_, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, initialRtvHeapSize),
    dsvHeap_(device_, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, initialDsvHeapSize),
    cbvSrvUavHeap_(device_, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, initialCbvSrvUavHeapSize),
    descRanges_(rtvHeap_, dsvHeap_, cbvSrvUavHeap_),
    window_(), fence_(device_) {
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

void Camera::update(float deltaTime) {
    auto playerXform = pAttached_->xform();
    auto playerPos = mu::Vec3(playerXform.row(3));

    auto cameraXform = camera_.coord().xform();
    auto cameraPos = cameraXform.row(3);

    mu::Mat4x4 rotationMatrix = mu::Mat4x4();
    mu::Vec3 right = pAttached_->xform().row(0);
    mu::Vec3 up = pAttached_->xform().row(1);
    mu::Vec3 look = pAttached_->xform().row(2);

    rotationMatrix.setRow(0, right);
    rotationMatrix.setRow(1, up);
    rotationMatrix.setRow(2, look); 

	mu::Vec3 xmf3Offset = mu::Vec4(offset_, 1.f) * rotationMatrix;
	mu::Vec3 xmf3Position = playerPos + xmf3Offset;
	mu::Vec3 xmf3Direction = xmf3Position - mu::Vec3(cameraPos);

    float fLength = xmf3Direction.len();
	xmf3Direction = xmf3Direction.norm();
	float fTimeLagScale = (timeLag_) ? deltaTime * (1.f / timeLag_) : 1.f;
	float fDistance = fLength * fTimeLagScale;

	if (fDistance > fLength) {
		fDistance = fLength;
	}

	if (fLength < 0.01f) {
		fDistance = fLength;
	}

	if (fDistance > 0.f) {
        cameraPos += mu::Vec4(xmf3Direction * fDistance, 0.f);
		auto posDiff = mu::Vec3(cameraPos) - xmf3Position;
		auto reducedDiff = posDiff * 0.5f;

        camera_.coord().setLocalXform(mu::Mat4x4());
        camera_.coord() << mu::translate(cameraPos);

        camera_.focusAt( playerPos + ( reducedDiff * mu::Vec3( playerXform.row(2) ) ), playerXform.row(1));
	}
	else {
        camera_.focusAt(playerPos + mu::Vec3(playerXform.row(2)), playerXform.row(1));
	}

	camera_.updateView();
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