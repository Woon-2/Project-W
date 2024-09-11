#include "SampleScene.hpp"

#include "d3d12res.hpp"

namespace gfx {

Generator<DrawInfo> SampleScene::iteration() const {
    auto mi = modelIteration(pModel_);
    for (auto& drawInfo : mi) {
        co_yield std::move(drawInfo);
    }
}

Generator<DrawInfo> SampleScene::modelIteration(const d3d12::Model* pModel) {
    auto world = pModel->coord().xform();

    for (const auto& mesh : pModel->meshes()) {
        auto drawInfo = DrawInfo();
        drawInfo.set(meshIdx, &mesh.mesh);
        drawInfo.set(worldIdx, world);
        co_yield drawInfo;
    }

    for (const auto& child : pModel->children()) {
        auto ci = modelIteration(&child);
        for (auto& drawInfo : ci) {
            co_yield std::move(drawInfo);
        }
    }
}

}   // namespace gfx