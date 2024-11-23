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

void Core::render(IRenderer& renderer, Scene& scene) {
    cmdList_.reset();
    cmdList_.get()->SetGraphicsRootSignature(root_.get().Get());
    cmdList_.get()->SetComputeRootSignature(root_.get().Get());
    window_.setRenderTarget(cmdList_);
    window_.clearRenderTarget(cmdList_);
    window_.clearDepthStencil(cmdList_);
    renderer.render(*this, scene);
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

void Core::loadRefModel( const std::filesystem::path& path,
    const d3d12::RefModelStorage::ID& key
) {
    refModelStorage_.loadModel(path, key, staticTexStorage_, device_, cmdList_);
}

void Core::loadTerrain( const d3d12::Bitmap& heightMap,
    const std::filesystem::path& albedoMapPath, const d3d12::RefModelStorage::ID& key,
    mu::Vec3 scale, std::size_t xDivisions, std::size_t zDivisions
) {
    if (!staticTexStorage_.contains(albedoMapPath)) {
        throw GFX_EXCEPT("Texture not found: " + albedoMapPath.string());
    }

    auto albedoMapRef = d3d12::Material::MapRef{
        .type = etoi(d3d12::Material::MapType::Albedo),
        .resourceIdx = static_cast<std::uint32_t>( staticTexStorage_.get(albedoMapPath).offset() ),
        .arrayIdx = 0u,
        .padding = 0u
    };

    for (std::size_t i = 0; i < zDivisions; ++i) {
        for (std::size_t j = 0; j < xDivisions; ++j) {
            auto serialKey = key + "_" + std::to_string(i) + "_" + std::to_string(j);

            refModelStorage_[serialKey] = d3d12::RefModel::loadTerrainSubsetFromHeightmap(
                heightMap, device_, cmdList_,
                static_cast<int>( (heightMap.width() / xDivisions) * j ),
                static_cast<int>( (heightMap.height() / zDivisions) * i ),
                static_cast<int>( (heightMap.width() / xDivisions) + 1 ),
                static_cast<int>( (heightMap.height() / zDivisions) + 1),
                scale, albedoMapRef
            );
        }
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

Model::Model( const ecs::Entity& entity,
    const d3d12::RefModelStorage::ID& key, const Core& core
) : ecs::Component(entity), model_(core.refModelStorage_.get(key)) {}

void Scene::addEntity(ecs::Entity& entity) {
    MyBase::addEntity(entity);
    reservedEntities_.push_back(entity.id().value());
}

void Scene::clearStash() {
    reservedEntities_.clear();
}

TerrainSubset::TerrainSubset( const d3d12::RefModelStorage::ID& key,
    Terrain* pTerrain, Core& core
) : key_(key), pTerrain_(pTerrain) {
    createComponent<Model>(key, core);
    createComponent<Coord>();
    as<Coord>().get().setParent(&pTerrain->as<Coord>().get());
}

TerrainSubset::TerrainSubset(TerrainSubset&& other) noexcept
    : Entity(std::move(other)), key_(std::move(other.key_)),
    pTerrain_(std::exchange(other.pTerrain_, nullptr)) {}

TerrainSubset& TerrainSubset::operator=(TerrainSubset&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    Entity::operator=(std::move(other));
    key_ = std::move(other.key_);
    pTerrain_ = std::exchange(other.pTerrain_, nullptr);

    return *this;
}

Terrain::Terrain( const d3d12::RefModelStorage::ID& identifier,
    const std::filesystem::path& heightMapPath,
    const std::filesystem::path& albedoMapPath, mu::Vec3 scale,
    Core& core, std::size_t xDivisions, std::size_t zDivisions
) : heightMap_(heightMapPath), subsets_(zDivisions), scale_(scale) {
    core.loadTerrain(heightMap_, albedoMapPath, identifier, scale, xDivisions, zDivisions);

    for (std::size_t i = 0; i < zDivisions; ++i) {
        for (std::size_t j = 0; j < xDivisions; ++j) {
            auto serialKey = identifier + "_" + std::to_string(i) + "_" + std::to_string(j);
            subsets_[i].emplace_back(serialKey, this, core);
        }
    }
}

Terrain::Terrain(Terrain&& other) noexcept
    : Entity(std::move(other)), heightMap_(std::move(other.heightMap_)),
    subsets_(std::move(other.subsets_)), scale_(std::move(other.scale_)) {

}

Terrain& Terrain::operator=(Terrain&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    Entity::operator=(std::move(other));
    heightMap_ = std::move(other.heightMap_);
    subsets_ = std::move(other.subsets_);
    scale_ = std::move(other.scale_);

    return *this;
}

namespace rp {

void PBRIllumination::init(Scene& scene) {
    for (auto& pModel : models(scene)) {
        trackModel(&pModel->get());
    }
    if (!cameras(scene).empty()) {
        setCamera(&cameras(scene).front()->get());
    }
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

void PBRIlluminationMacro::init(Scene& scene) {
    for (auto& pModel : models(scene)) {
        if (pModel) {
            trackModel(&pModel->get());
        }
    }
    if (!cameras(scene).empty()) {
        auto& pCamera = cameras(scene).front();
        if (pCamera) {
            setCamera(&cameras(scene).front()->get());
        }
    }
}

void PBRIlluminationMacro::update(Scene& scene) {
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