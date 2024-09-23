#ifndef __Client_Model_HPP
#define __Client_Model_HPP

#include "ecs.hpp"
#include "coord.hpp"

#include "d3d12model.hpp"
#include "d3d12materialTree.hpp"

#include <vector>
#include <optional>

class ModelData {
public:
    ModelData(const gfx::d3d12::Model* srcModel, const gfx::d3d12::MaterialTree* srcMaterialTree);

    void setCoordSys(const gfx::coord::System& coordSys) { coordSys_ = coordSys; }
    gfx::coord::System& coord() NOEXCEPT { return coordSys_; }
    const gfx::coord::System& coord() const NOEXCEPT { return coordSys_; }

    const gfx::d3d12::Model* srcModel() const NOEXCEPT { return srcModel_; }
    const gfx::d3d12::MaterialTree* srcMaterialTree() const NOEXCEPT { return srcMaterialTree_; }

    const auto& children() const NOEXCEPT { return children_; }
    auto& children() NOEXCEPT { return children_; }

    void addChild(const ModelData& child) { children_.push_back(child); }
    void addChild(ModelData&& child) { children_.push_back(std::move(child)); }
    void emplaceChild( const gfx::d3d12::Model* srcModel,
        const gfx::d3d12::MaterialTree* srcMaterialTree
    ) {
        children_.emplace_back(srcModel, srcMaterialTree);
    }

private:
    static void syncHierarchy( const gfx::d3d12::Model& srcSubModel,
        const gfx::d3d12::MaterialTree srcSubMaterialTree, ModelData& dstModel
    );

    gfx::coord::System coordSys_;
    std::vector<ModelData> children_;
    const gfx::d3d12::Model* srcModel_;
    const gfx::d3d12::MaterialTree* srcMaterialTree_;
};

class Model : public ecs::Component {
public:
    ENABLE_COMPONENT(Model);

    Model(const ecs::Entity& entity) NOEXCEPT
        : ecs::Component(entity) {}

    void init(const gfx::d3d12::Model* srcModel, const gfx::d3d12::MaterialTree* srcMaterialTree) {
        modelData_ = ModelData(srcModel, srcMaterialTree);
    }
    bool valid() const NOEXCEPT { return modelData_.has_value(); }

private:
    std::optional<ModelData> modelData_;
};

#endif // __Client_Model_HPP