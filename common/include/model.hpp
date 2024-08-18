#ifndef __MODEL_HPP
#define __MODEL_HPP

#define ASSIMP_MATH_UTIL

#include "mesh.hpp"
#include "inputLayout.hpp"

#include "coord.hpp"

#include <vector>
#include <string>
#include <string_view>
#include <algorithm>
#include <filesystem>

namespace gfx {

/**
 * @brief A class representing a model independent of rendering API.     
 * Model structures a tree of meshes(scene graph).     
 * Each model becomes a node in the tree and is related to another model through a relative coordinate system.     
 * @details Every single model has its own coordinate system.    
 * A child Model's total transformation is the product of its parent's total transformation and its own local transformation.
 * @see Mesh coord::System loadModel
 */
class Model {
private:
    struct NamedMesh {
        Mesh mesh;
        std::string name;

        friend auto operator<=>(const NamedMesh& lhs, const NamedMesh& rhs) noexcept {
            return lhs.name <=> rhs.name;
        }
    };

public:
    /**
     * @brief Creates an empty model with a name.    
     * The model has no children and no meshes.
     * @param name The name of the model.
     * @note The name is used to identify the model.
     * @see Model::name Model::child
     */
    Model(const std::string& name)
        : coordSys_(), children_(), meshes_(), name_(name) {}

    std::string_view name() const NOEXCEPT {
        return name_;
    }

    /**
     * @brief Adds a child model to the model.     
     * The child model's coordinate system sets its parent to the caller model's coordinate system.
     * @param child The child model to add.
     * @see Model::popChild Model::child coord::System::setParent
     */
    void addChild(const Model& child) {
        children_.push_back(child);
        children_.back().coord().setParent(&coordSys_);
    }
    void addChild(Model&& child) {
        children_.push_back(std::move(child));
        children_.back().coord().setParent(&coordSys_);
    }
    /**
     * @brief Adds a child model to the model with a transformation.     
     * The child model's coordinate system sets its parent to the caller model's coordinate system,    
     * and the transformation matrix is applied to the child model's local transformation.
     * @param child The child model to add.
     * @param xform The transformation matrix.
     * @see Model::popChild Model::child coord::System::setParent
     */
    void MU_CALLCONV addChild(Model&& child, mu::Mat4x4 xform) {
        children_.push_back(std::move(child));
        children_.back().coord().setParent(&coordSys_);
        children_.back().coord() << xform;
    }

    /**
     * @brief Pops a child model from the model via searching by name.
     * @throws `std::runtime_error` If the child model is not found.
     * @param name The name of the child model to remove.
     * @return Model The popped child model.
     * @see Model::addChild Model::child
     */
    Model popChild(std::string_view name);
    /**
     * @brief Accesses a child model by name.
     * @throws `std::runtime_error` If the child model is not found.
     * @param name The name of the child model to access.
     * @return const Model& A const reference to the child model.
     * @see Model::popChild Model::addChild
     */
    const Model& child(std::string_view name) const;
    Model& child(std::string_view name) {
        return const_cast<Model&>( const_cast<const Model*>(this)->child(name) );
    }
    /**
     * @brief Adds a mesh to the model with a name.
     * @param mesh The mesh to add.
     * @param name The name of the mesh.
     * @note The name is used to identify the mesh.
     * @see Model::popMesh Model::mesh
     */
    void addMesh(const Mesh& mesh, const std::string& name) {
        meshes_.push_back({mesh, name});
    }
    void addMesh(Mesh&& mesh, const std::string& name) {
        meshes_.push_back({std::move(mesh), name});
    }
    void addMesh(const Mesh& mesh, std::string&& name) {
        meshes_.push_back({mesh, std::move(name)});
    }
    void addMesh(Mesh&& mesh, std::string&& name) {
        meshes_.push_back({std::move(mesh), std::move(name)});
    }

    /**
     * @brief Pops a mesh from the model via searching by name.
     * @throws `std::runtime_error` If the mesh is not found.
     * @param name The name of the mesh to remove.
     * @return Mesh The popped mesh.
     * @see Model::addMesh Model::mesh
     */
    Mesh popMesh(std::string_view name);
    /**
     * @brief Accesses a mesh by name.
     * @throws `std::runtime_error` If the mesh is not found.
     * @param name The name of the mesh to access.
     * @return const Mesh& A const reference to the mesh.
     * @see Model::popMesh Model::addMesh
     */
    const Mesh& mesh(std::string_view name) const;
    Mesh& mesh(std::string_view name) {
        return const_cast<Mesh&>( const_cast<const Model*>(this)->mesh(name) );
    }

    /**
     * @brief Sets the coordinate system of the model by copying the coordinate system.
     * @param coordSys The coordinate system to set.
     * @see coord::System
     */
    void setCoord(const coord::System& coordSys) {
        coordSys_ = coordSys;
    }
    /**
     * @brief Accesses the coordinate system of the model.
     * @return const coord::System& A const reference to the coordinate system.
     * @see coord::System
     */
    const coord::System& coord() const NOEXCEPT {
        return coordSys_;
    }
    coord::System& coord() NOEXCEPT {
        return coordSys_;
    }

    auto& children() NOEXCEPT {
        return children_;
    }

    const auto& children() const NOEXCEPT {
        return children_;
    }

    auto& meshes() NOEXCEPT {
        return meshes_;
    }

    const auto& meshes() const NOEXCEPT {
        return meshes_;
    }

private:
    coord::System coordSys_;
    std::vector<Model> children_;
    std::vector<NamedMesh> meshes_;
    std::string name_;
};

/**
 * @brief Loads a model tree from a file.    
 * It loads meshes with assimp in the way loadModel does.    
 * And the tree is structured by the scene graph in the file.
 * @param path Path to the file.
 * @return Model A model object representing the scene graph.
 * @see Model loadMesh coord::System
 */
Model loadModel(const std::filesystem::path& path);
/**
 * @brief Loads a model tree from a file with a specified input layout.    
 * It loads meshes with assimp in the way loadModel does.    
 * And the tree is structured by the scene graph in the file.
 * @param path Path to the file.
 * @param il The input layout to set to the vertex buffer.
 * @return Model A model object representing the scene graph.
 * @see Model loadMesh coord::System VertexBuffer InputLayout convert
 */
Model loadModel(const std::filesystem::path& path, const InputLayout& il);

}   // namespace gfx

#endif // __MODEL_HPP