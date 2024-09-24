#include "cmodel.hpp"

#include <ranges>
#include <algorithm>

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

std::vector<gfx::d3d12::Fragment> Fragmentizer::fragmentize() const {
    std::vector<gfx::d3d12::Fragment> fragments;
    for (const auto& [key, models] : instanceSets_) {
        auto nodes = std::vector<const ModelData*>();
        nodes.reserve(models.size());

        std::ranges::for_each( models, [&nodes](const auto& model) {
            if (auto pModel = model.lock(); pModel) {
                nodes.push_back(&pModel->root());
            }
        } );

        fragmentizeNodes(
            std::get<const gfx::d3d12::Model*>(key),
            std::get<const gfx::d3d12::MaterialTree*>(key),
            nodes, fragments
        );
    }

    return fragments;
}

void Fragmentizer::fragmentizeNodes( const gfx::d3d12::Model* refModel,
    const gfx::d3d12::MaterialTree* refMatTree,
    const std::vector<const ModelData*>& nodes,
    std::vector<gfx::d3d12::Fragment>& fragments
) const {
    /*
    노드 처리(레퍼런스노드포인터, 노드포인터벡터):
    노드포인터벡터를 월드변환벡터로 변환한다.
    레퍼런스노드포인터의 각 메시에 대해:
        해당 메시와 월드변환벡터로 Fragment를 구성해 Scene에 삽입한다.
    노드포인터벡터를 자식반복자벡터로 변환한다.
    레퍼런스노드포인터의 각 자식 노드에 대해:
        자식반복자벡터를 자식노드포인터벡터로 변환한다.
        노드 처리(레퍼런스자식노드포인터, 자식노드포인터벡터)
        반복자벡터의 반복자들을 전진시킨다.
    */

   // TODO: 트리들의 모양이 정확히 같은지 체크

    auto worlds = std::vector<mu::Mat4x4>();
    worlds.reserve(nodes.size());
    std::transform( nodes.begin(), nodes.end(), std::back_inserter(worlds),
        [](const ModelData* node) {
            return node->coord().xform();
        }
    );

    auto meshIt = refModel->meshes().begin();
    auto matIt = refMatTree->materials().begin();

    while (meshIt != refModel->meshes().end()) {
        fragments.emplace_back(
            /* .pMesh = */ &meshIt->mesh,
            /* .pMaterial = */ &(*matIt),
            /* .worlds = */ worlds
        );

        ++meshIt;
        ++matIt;
    }

    auto childIts = std::vector< std::vector<ModelData>::const_iterator >();
    childIts.reserve(nodes.size());
    for (const auto& node : nodes) {
        childIts.push_back(node->children().begin());
    }

    auto modelIt = refModel->children().begin();
    auto matTreeIt = refMatTree->children().begin();

    while (modelIt != refModel->children().end()) {
        auto childNodes = std::vector<const ModelData*>();
        childNodes.reserve(nodes.size());

        std::ranges::transform( childIts, std::back_inserter(childNodes),
            [](const auto& it) { return &*it; }
        );

        fragmentizeNodes(&*modelIt, &*matTreeIt, childNodes, fragments);

        std::ranges::for_each( childIts, [](auto& it) {
            ++it;
        } );

        ++modelIt;
        ++matTreeIt;
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

    auto& root = model->root();
    instanceSets_[ {root.srcModel(), root.srcMaterialTree()} ]
        .push_back(std::move(weakModel));
}