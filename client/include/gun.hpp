#ifndef __GUN_HPP
#define __GUN_HPP

#include "coord.hpp"
#include "drawInfo.hpp"

#include "d3d12mesh.hpp"
#include "d3d12material.hpp"

class Gun {
public:
    Gun(gfx::coord::System* pParentCoord)
        : coord_()/*, mesh_(&sMesh), material_(&sMaterial) */ {
        coord_.setParent(pParentCoord);
    }
    void MU_CALLCONV update(mu::Vec3 pos, mu::NQuat rot);
    mu::Mat4x4 MU_CALLCONV world() const { return coord_.xform(); }
    gfx::coord::System& coord() { return coord_; }

private:
    gfx::coord::System coord_;
    gfx::d3d12::Mesh* mesh_;
    gfx::d3d12::Material* material_;
};

#endif // __GUN_HPP