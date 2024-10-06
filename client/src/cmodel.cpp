#include "cmodel.hpp"

#include <ranges>
#include <algorithm>

void ModelDataXX::syncHierarchy( const gfx::d3d12::Model& srcSubModel,
    const gfx::d3d12::MaterialTree& srcSubMaterialTree, ModelDataXX& dstModel
) {
    dstModel.children_.clear();
    dstModel.nodes_.clear();
    dstModel.treeSize_ = 0;

    auto itMesh = srcSubModel.meshes().begin();
    auto itMaterial = srcSubMaterialTree.materials().begin();

    while (itMesh != srcSubModel.meshes().end()) {
        dstModel.nodes_.emplace_back( &itMesh->mesh, &*itMaterial, &dstModel );
        ++dstModel.treeSize_;

        ++itMesh;
        ++itMaterial;
    }

    auto itModel = srcSubModel.children().begin();
    auto itMatTree = srcSubMaterialTree.children().begin();

    while (itModel != srcSubModel.children().end()) {
        dstModel.children_.emplace_back(&*itModel, &*itMatTree);
        dstModel.children_.back().coord().setParent(&dstModel.coord());

        dstModel.treeSize_ += dstModel.children_.back().treeSize();

        ++itModel;
        ++itMatTree;
    }
}

ModelData::ModelData(const gfx::d3d12::Model* srcModel, const gfx::d3d12::MaterialTree* srcMaterialTree)
    : coordSys_(srcModel->coord()), srcModel_(srcModel), srcMaterialTree_(srcMaterialTree) {
    syncHierarchy(*srcModel, *srcMaterialTree, *this);
}

void ModelData::syncHierarchy( const gfx::d3d12::Model& srcSubModel,
    const gfx::d3d12::MaterialTree srcSubMaterialTree, ModelData& dstModel
) {
    auto itModel = srcSubModel.children().begin();
    auto itMaterial = srcSubMaterialTree.children().begin();

    while (itModel != srcSubModel.children().end()) {
        dstModel.emplaceChild(&*itModel, &*itMaterial);
        dstModel.children().back().coord().setParent(&dstModel.coord());

        syncHierarchy(*itModel, *itMaterial, dstModel.children().back());

        ++itModel;
        ++itMaterial;
    }
}

void Fragmentizer::addEntity(ecs::Entity& entity) {
    ecs::System<Model>::addEntity(entity);
    auto weakModel = entity.getC<Model>();
    auto model = weakModel.lock();
    if (!model) {
        throw ECS_EXCEPT("Entity does not have a Model component");
    }

    if (!model->valid()) {
        throw std::runtime_error("Model component is not initialized");
    }

    addModelData(model->root());
}

void Fragmentizer::addModelData(const ModelDataXX& modelData) {
    for ( const auto& node : modelData.nodes() ) {
        nodesMap_[node].push_back(&node);
    }

    for ( const auto& child : modelData.children() ) {
        addModelData(child);
    }
}

std::vector<gfx::d3d12::Fragment> Fragmentizer::fragmentize(
    std::vector<mu::Mat4x4>& outWorlds
) const {
    std::vector<gfx::d3d12::Fragment> fragments;

    const auto fragmentCntExpected = nodesMap_.size();
    std::size_t worldCntExpected = 0;

    for (const auto& [key, nodes] : nodesMap_) {
        worldCntExpected += nodes.size();
    }

    fragments.reserve(fragmentCntExpected);
    outWorlds.reserve(worldCntExpected);

    for (const auto& [key, nodes] : nodesMap_) {
        fragmentizeData(key, nodes, fragments, outWorlds);
    }

    return fragments;
}

void Fragmentizer::fragmentizeData(
    const ModelDataXX::Node& key,
    const std::vector<const ModelDataXX::Node*>& data,
    std::vector<gfx::d3d12::Fragment>& fragments,
    std::vector<mu::Mat4x4>& worlds
) const {
    if (data.empty()) {
        return;
    }

    auto oldWorldCnt = worlds.size();

    std::transform(data.begin(), data.end(), std::back_inserter(worlds),
        [](const ModelDataXX::Node* node) {
            return node->pModel->coord().xform();
        }
    );

    for (const auto& node : data) {
        fragments.emplace_back(
            /* .pMesh = */ node->meshView,
            /* .pMaterial = */ node->pMaterial,
            /* .worlds = */ std::span( worlds.data() + oldWorldCnt, worlds.data() + worlds.size() )
        );
    }
}