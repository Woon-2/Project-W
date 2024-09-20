#include "d3d12model.hpp"

namespace gfx {

namespace d3d12 {

void Model::completeInit() {
    for (auto& mesh : meshes()) {
        mesh.mesh.completeInit();
    }
    for (auto& child : children()) {
        child.completeInit();
    }
}

}   // namespace d3d12

}   // namespace gfx