#ifndef __Client_Model_HPP
#define __Client_Model_HPP

#include "coord.hpp"

#include "d3d12model.hpp"
#include "d3d12materialTree.hpp"

#include <vector>

class Model {
public:
    Model(const gfx::d3d12::Model* srcModel, const gfx::d3d12::MaterialTree* srcMaterialTree);

    void setCoordSys(const gfx::coord::System& coordSys) { coordSys_ = coordSys; }
    gfx::coord::System& coord() NOEXCEPT { return coordSys_; }
    const gfx::coord::System& coord() const NOEXCEPT { return coordSys_; }

    const gfx::d3d12::Model* srcModel() const NOEXCEPT { return srcModel_; }
    const gfx::d3d12::MaterialTree* srcMaterialTree() const NOEXCEPT { return srcMaterialTree_; }

    const auto& children() const NOEXCEPT { return children_; }
    auto& children() NOEXCEPT { return children_; }

    void addChild(const Model& child) { children_.push_back(child); }
    void addChild(Model&& child) { children_.push_back(std::move(child)); }
    void emplaceChild( const gfx::d3d12::Model* srcModel,
        const gfx::d3d12::MaterialTree* srcMaterialTree
    ) {
        children_.emplace_back(srcModel, srcMaterialTree);
    }

private:
    static void syncHierarchy( const gfx::d3d12::Model& srcSubModel,
        const gfx::d3d12::MaterialTree srcSubMaterialTree, Model& dstModel
    );

    gfx::coord::System coordSys_;
    std::vector<Model> children_;
    const gfx::d3d12::Model* srcModel_;
    const gfx::d3d12::MaterialTree* srcMaterialTree_;
};

#endif // __Client_Model_HPP