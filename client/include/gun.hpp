#ifndef __GUN_HPP
#define __GUN_HPP

#include "coord.hpp"
#include "drawInfo.hpp"

#include "d3d12mesh.hpp"
#include "d3d12material.hpp"

class Gun {
public:
    static gfx::d3d12::Mesh sMesh;
    static gfx::d3d12::Material sMaterial;

    Gun(gfx::coord::System* pParentCoord)
        : coord_(), mesh_(&sMesh), material_(&sMaterial) {
        coord_.setParent(pParentCoord);
    }

    static void loadAssets(gfx::d3d12::Core& core, std::size_t fenceIdx = 0u);
    void MU_CALLCONV update(mu::Vec3 pos, mu::NQuat rot);
    mu::Mat4x4 MU_CALLCONV world() const { return coord_.xform(); }

private:
    gfx::coord::System coord_;
    gfx::d3d12::Mesh* mesh_;
    gfx::d3d12::Material* material_;
};

#endif // __GUN_HPP