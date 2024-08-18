#ifndef __D3D12MODEL_HPP
#define __D3D12MODEL_HPP

#include "model.hpp"

#include "d3d12core.hpp"
#include "d3d12mesh.hpp"

#include "coord.hpp"

#include <vector>
#include <string>
#include <memory>

namespace gfx {

namespace d3d12 {

/**
 * @brief A class representing a model in D3D12.     
 * Like gfx::Model, it structures a tree of meshes(scene graph), except that the contained meshes are Mesh, rather than gfx::Mesh.     
 * For further information, see gfx::Model and Mesh.
 * @see gfx::Model Mesh
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
     * @brief keeping same structure as gfx::Model, it converts the gfx::Model's contained meshes to Mesh es.
     * @param core The D3D12 core object.
     * @param ctx The D3D12 render context object.
     * @param model The gfx::Model to convert.
     * @param vbUpIdx The upload buffer index for the vertex buffer, which is used to register and to pop the upload buffer from the core.
     * @param ibUpIdx The upload buffer index for the index buffer, which is used to register and to pop the upload buffer from the core.
     * @details As the Mesh isn't in valid state until the gpu actually uploads the data,     
     * the Model also isn't in valid state until the gpu actually uploads the data.     
     * 
     * The temporary upload buffer indices for the vertex buffer and the index buffer is serialized and shared among the models in following way:    
     * If the model is the root model to be created, the upload buffer index for it is suffixed with 0.    
     * Then, the suffix number is incremented for each model visited while traversing the model tree,    
     * reaching the number of (the count of models in the tree - 1).
     * 
     * for example, if the model tree is like below and `vbUpIdx` is "vb" and `ibUpIdx` is "ib":    
     * root                : vb0, ib0    
     * ├── child1          : vb1, ib1    
     * │   ├── child1-1    : vb2, ib2     
     * │   └── child1-2    : vb3, ib3    
     * └── child2          : vb4, ib4    
     * 
     * the upload buffer indices for the root, child1, child1-1, child1-2, and child2 are    
     * "vb0"&"ib0", "vb1"&"ib1", "vb2"&"ib2", "vb3"&"ib3", and "vb4"&"ib4" respectively.
     * @see Mesh gfx::Model Core Core::addTmpUpBuf Core::popTmpUpBuf Core::popTmpUpBufs
     */
    Model( d3d12::Core& core, d3d12::D3D12RenderContext& ctx, const gfx::Model& model,
        Core::UpBufIdx vbUpIdx, Core::UpBufIdx ibUpIdx
    ) : Model(core, ctx, model, vbUpIdx, ibUpIdx, std::make_shared<std::size_t>(0), std::make_shared<std::size_t>(0)) {}

    /**
     * @brief Calls completeInit for all the meshes in the model.
     * @param core The D3D12 core object.
     * @see Mesh::completeInit
     */
    void completeInit(d3d12::Core& core) const;

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

    /**
     * @brief Get a reference to the children container of the model.     
     * It is guaranteed that the children container satisfies std::ranges::range concept.
     */
    auto& children() NOEXCEPT {
        return children_;
    }
    const auto& children() const NOEXCEPT {
        return children_;
    }
    /**
     * @brief Get a reference to the meshes container of the model.     
     * It is guaranteed that the meshes container satisfies std::ranges::range concept.
     * @note The contained meshes are wrappers of the Mesh es which is attached a name.    
     * To access the Mesh, access to `mesh` member of the element of the container.
     */
    auto& meshes() NOEXCEPT {
        return meshes_;
    }
    const auto& meshes() const NOEXCEPT {
        return meshes_;
    }

private:
    Model( d3d12::Core& core, d3d12::D3D12RenderContext& ctx, const gfx::Model& model,
        Core::UpBufIdx vbUpIdx, Core::UpBufIdx ibUpIdx,
        std::shared_ptr<std::size_t> pVbSerialIdx, std::shared_ptr<std::size_t> ibSerialIdx
    );

    Core::UpBufIdx serializeVbIdx(const Core::UpBufIdx& idx);
    Core::UpBufIdx serializeIbIdx(const Core::UpBufIdx& idx);

    coord::System coordSys_;
    std::vector<Model> children_;
    std::vector<NamedMesh> meshes_;
    std::string name_;
    std::shared_ptr<std::size_t> pVbSerialIdx_;
    std::shared_ptr<std::size_t> pIbSerialIdx_;
};

}   // namespace gfx::d3d12

}   // namespace gfx

#endif // __D3D12MODEL_HPP