#include "d3d12scene.hpp"

namespace gfx {

namespace d3d12 {

Generator<DrawInfo> CameraScene::iteration() const {
    for (const auto& model : models_) {
        auto mi = modelIteration(&model);
        for (auto& drawInfo : mi) {
            co_yield std::move(drawInfo);
        }
    }
}

Generator<DrawInfo> CameraScene::modelIteration(const d3d12::Model* pModel) const {
    auto world = pModel->coord().totalXform();

    for (const auto& mesh : pModel->meshes()) {
        auto drawInfo = DrawInfo();
        drawInfo.set(meshIdx, &mesh.mesh);
        drawInfo.set(worldIdx, world);
        fillViewProj(drawInfo);
        co_yield drawInfo;
    }

    for (const auto& child : pModel->children()) {
        auto ci = modelIteration(&child);
        for (auto& drawInfo : ci) {
            co_yield std::move(drawInfo);
        }
    }
}

}   // namespace d3d12

}   // namespace gfx