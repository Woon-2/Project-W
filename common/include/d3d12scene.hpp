#ifndef __D3D12_SCENE_HPP
#define __D3D12_SCENE_HPP

#include "camera.hpp"
#include "d3d12core.hpp"

#include "d3d12model.hpp"

#include <vector>

namespace gfx {

namespace d3d12 {

class CameraScene : public gfx::CameraScene {
public:
    static constexpr std::size_t meshIdx = gfx::CameraScene::meshIdx;
    static constexpr std::size_t worldIdx = gfx::CameraScene::worldIdx;
    static constexpr std::size_t viewIdx = gfx::CameraScene::viewIdx;
    static constexpr std::size_t projIdx = gfx::CameraScene::projIdx;
    static constexpr std::size_t colorIdx = 4u;

    CameraScene(const Camera& camera)
        : gfx::CameraScene(camera) {}

    Generator<DrawInfo> iteration() const override;

    void addModel(const d3d12::Model& model) {
        models_.push_back(model);
    }

    void addModel(d3d12::Model&& model) {
        models_.push_back(std::move(model));
    }

private:
    Generator<DrawInfo> modelIteration(const d3d12::Model* pModel) const;

    std::vector<d3d12::Model> models_;
};

}   // namespace d3d12

}   // namespace gfx

#endif // __D3D12_SCENE_HPP