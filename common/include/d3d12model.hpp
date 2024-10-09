#ifndef __D3D12MODEL_HPP
#define __D3D12MODEL_HPP

#include "model.hpp"

#include "d3d12core.hpp"
#include "d3d12mesh.hpp"

#include "coord.hpp"

#include <vector>
#include <string>
#include <memory>

#include "config.hpp"

namespace gfx {

namespace d3d12 {
/**
 * @brief A class representing a model in D3D12.     
 * Like gfx::Model, it structures a tree of meshes(scene graph), except that the contained meshes are Mesh, rather than gfx::Mesh.     
 * For further information, see gfx::Model and Mesh.
 * @see gfx::Model Mesh
 */
class Model : public gfx::Model<Model, Mesh> {
public:
    using MeshType = Mesh;
    using gfx::Model<Model, Mesh>::Model;
    /**
     * @brief Calls completeInit for all the meshes in the model.
     * @param core The D3D12 core object.
     * @see Mesh::completeInit
     */
    void completeInit();
};

}   // namespace gfx::d3d12

}   // namespace gfx

#endif // __D3D12MODEL_HPP