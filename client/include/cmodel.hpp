#ifndef __Client_Model_HPP
#define __Client_Model_HPP

#include "ecs.hpp"
#include "coord.hpp"

#include "d3d12model.hpp"
#include "d3d12materialTree.hpp"
#include "drawInfo.hpp"

#include <vector>
#include <optional>
#include <tuple>
#include <numeric>
#include <set>

class ModelDataXX {
public:
    struct Node {
        gfx::d3d12::MeshView meshView;
        const gfx::d3d12::Material* pMaterial;
        const ModelDataXX* pModel;

        auto operator<=>(const Node& other) const NOEXCEPT {
            auto first = meshView <=> other.meshView;
            if (first != 0) return first;
            return pMaterial <=> other.pMaterial;
        }
    };

    ModelDataXX(const gfx::d3d12::Model* srcModel, const gfx::d3d12::MaterialTree* srcMaterialTree)
        : coordSys_(srcModel->coord()), nodes_(), children_(), treeSize_{},
        srcModel_(srcModel), srcMaterialTree_(srcMaterialTree) {
        syncHierarchy(*srcModel, *srcMaterialTree, *this);
    }

    void setCoordSys(const gfx::coord::System& coordSys) { coordSys_ = coordSys; }
    gfx::coord::System& coord() NOEXCEPT { return coordSys_; }
    const gfx::coord::System& coord() const NOEXCEPT { return coordSys_; }

    const auto& nodes() const NOEXCEPT { return nodes_; }
    auto& nodes() NOEXCEPT { return nodes_; }

    const auto& children() const NOEXCEPT { return children_; }
    auto& children() NOEXCEPT { return children_; }

    std::size_t treeSize() const NOEXCEPT {
        return treeSize_;
    }

    const gfx::d3d12::Model* srcModel() const NOEXCEPT { return srcModel_; }
    const gfx::d3d12::MaterialTree* srcMaterialTree() const NOEXCEPT { return srcMaterialTree_; }

private:
    static void syncHierarchy( const gfx::d3d12::Model& srcSubModel,
        const gfx::d3d12::MaterialTree& srcSubMaterialTree, ModelDataXX& dstModel
    );

    gfx::coord::System coordSys_;
    std::vector<Node> nodes_;
    std::vector<ModelDataXX> children_;
    std::size_t treeSize_;
    const gfx::d3d12::Model* srcModel_;
    const gfx::d3d12::MaterialTree* srcMaterialTree_;
};

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

    std::size_t treeSize() const NOEXCEPT {
        return std::accumulate( children_.begin(), children_.end(),
            srcModel_->meshes().size(),
            [](std::size_t sum, const ModelData& child) {
                return sum + child.treeSize();
            }
        );
    }

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
        modelData_ = ModelDataXX(srcModel, srcMaterialTree);
    }
    bool valid() const NOEXCEPT { return modelData_.has_value(); }

    std::size_t treeSize() const NOEXCEPT { return modelData_->treeSize(); }
    const ModelDataXX& root() const NOEXCEPT { return modelData_.value(); }
    ModelDataXX& root() NOEXCEPT { return modelData_.value(); }

private:
    std::optional<ModelDataXX> modelData_;
};

class Fragmentizer : public ecs::System<Model> {
public:
    std::vector<gfx::d3d12::Fragment> fragmentize(std::vector<mu::Mat4x4>& outWorlds) const;
    void addEntity(ecs::Entity& entity);

private:
    void addModelData(const ModelDataXX& modelData);

    void fragmentizeData( 
        const ModelDataXX::Node& key,
        const std::vector<const ModelDataXX::Node*>& data,
        std::vector<gfx::d3d12::Fragment>& fragments,
        std::vector<mu::Mat4x4>& worlds
    ) const;

    void fragmentizeData( 
        const std::vector<const ModelDataXX*>& data,
        std::vector<gfx::d3d12::Fragment>& fragments,
        std::vector<mu::Mat4x4>& worlds
    ) const;

    std::map< const ModelDataXX::Node, std::vector<const ModelDataXX::Node*> > nodesMap_;
};

#endif // __Client_Model_HPP