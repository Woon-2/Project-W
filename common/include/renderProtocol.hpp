#ifndef __RENDERPROTOCOL_HPP
#define __RENDERPROTOCOL_HPP

#include "shaderRes.hpp"

namespace gfx {

namespace rp {

enum class Protocol {
    PhongInstancingNT,
    PhongInstancing,
    SomeProtocol,
};

enum class DIType {
    Mesh,
    PID,
    PDD,
    PFD,
    Light,
    Material
};

struct PhongInstancingNT {
    static constexpr std::size_t typeIdx = 0u;
    static constexpr std::size_t meshIdx = 1u;
    static constexpr std::size_t PIDIdx = 2u;
    static constexpr std::size_t PDDIdx = 3u;
    static constexpr std::size_t PFDIdx = 4u;
    static constexpr std::size_t lightIdx = 5u;
    static constexpr std::size_t materialIdx = 6u;

    using PIDType = d3d12::sr::BasicPID;
    using PDDType = d3d12::sr::BasicPDD;
    using PFDType = d3d12::sr::BasicPFD;
    using LightType = d3d12::sr::PhongLight;
    using MaterialType = d3d12::sr::PhongMaterialNT;
};

struct PhongInstancing {
    static constexpr std::size_t typeIdx = 0u;
    static constexpr std::size_t meshIdx = 1u;
    static constexpr std::size_t PIDIdx = 2u;
    static constexpr std::size_t PDDIdx = 3u;
    static constexpr std::size_t PFDIdx = 4u;
    static constexpr std::size_t lightIdx = 5u;
    static constexpr std::size_t materialIdx = 6u;

    using PIDType = d3d12::sr::BasicPID;
    using PDDType = d3d12::sr::BasicPDD;
    using PFDType = d3d12::sr::BasicPFD;
    using LightType = d3d12::sr::PhongLight;
    using MaterialType = d3d12::sr::PhongMaterial;
};

}   // namespace gfx::rp

}   // namespace gfx

#endif // !__RENDERPROTOCOL_HPP



