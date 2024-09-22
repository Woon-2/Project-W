#include "cmodel.hpp"

Model::Model(const gfx::d3d12::Model* srcModel, const gfx::d3d12::MaterialTree* srcMaterialTree)
    : coordSys_(srcModel->coord()), srcModel_(srcModel), srcMaterialTree_(srcMaterialTree) {
    syncHierarchy(*srcModel, *srcMaterialTree, *this);
}

void Model::syncHierarchy( const gfx::d3d12::Model& srcSubModel,
    const gfx::d3d12::MaterialTree srcSubMaterialTree, Model& dstModel
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