#ifndef __SAMPLE_SCENE_HPP
#define __SAMPLE_SCENE_HPP

#include "d3d12core.hpp"

#include "gfxPrimitive.hpp"

#include "d3d12model.hpp"
#include "d3d12InputLayoutPresets.hpp"
#include "resourcePath.hpp"

#include <array>

namespace gfx {

class SampleScene : public IScene {
public:
    static constexpr std::size_t meshIdx = 0u;
    static constexpr std::size_t worldIdx = 1u;

    SampleScene(const d3d12::Model& model)
        : pModel_(&model) {}

    Generator<DrawInfo> iteration() const override;

private:
    static Generator<DrawInfo> modelIteration(const d3d12::Model* pModel);

    const d3d12::Model* pModel_;
};

} // namespace gfx

#endif // __SAMPLE_SCENE_HPP