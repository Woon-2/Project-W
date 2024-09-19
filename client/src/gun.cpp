#include "gun.hpp"

#include "resourcePath.hpp"
#include "d3d12InputLayoutPresets.hpp"

#include <ranges>

void MU_CALLCONV Gun::update(mu::Vec3 pos, mu::NQuat rot) {
    coord_.setLocalXform(rot.mat4() * mu::translate(pos));
}