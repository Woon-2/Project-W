#include "model.hpp"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

namespace gfx {

template <class ConcreteModel, class ConcreteMesh>
const ConcreteModel& Model<ConcreteModel, ConcreteMesh>::child(std::string_view name) const {
    auto it = std::find_if( children_.begin(), children_.end(), [&name](const Model& child) {
        return child.name() == name;
    } );

    if (it == children_.end()) {
        throw std::runtime_error("Child not found");
    }

    return *it;
}

template <class ConcreteModel, class ConcreteMesh>
const ConcreteModel Model<ConcreteModel, ConcreteMesh>::popChild(std::string_view name) {
    auto retPos = std::remove_if( children_.begin(), children_.end(),
        [&name](const Model& child) {
            return child.name() == name;
        }
    );

    if (retPos == children_.end()) {
        throw std::runtime_error("Child not found");
    }

    auto ret = std::move(*retPos);

    children_.erase(retPos, children_.end());

    return ret;
}

template <class ConcreteModel, class ConcreteMesh>
const ConcreteMesh& Model<ConcreteModel, ConcreteMesh>::mesh(std::string_view name) const {
    auto it = std::find_if( meshes_.begin(), meshes_.end(), [&name](const NamedMesh& nm) {
        return nm.name == name;
    } );

    if (it == meshes_.end()) {
        throw std::runtime_error("Mesh not found");
    }

    return it->mesh;
}

template <class ConcreteModel, class ConcreteMesh>
const ConcreteMesh Model<ConcreteModel, ConcreteMesh>::popMesh(std::string_view name) {
    auto retPos = std::remove_if( meshes_.begin(), meshes_.end(),
        [&name](const NamedMesh& nm) {
            return nm.name == name;
        }
    );

    if (retPos == meshes_.end()) {
        throw std::runtime_error("Mesh not found");
    }

    auto ret = std::move(retPos->mesh);

    meshes_.erase(retPos, meshes_.end());

    return ret;
}

}   // namespace gfx