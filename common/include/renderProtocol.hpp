#ifndef __RENDERPROTOCOL_HPP
#define __RENDERPROTOCOL_HPP

#include "shaderRes.hpp"

#include <vector>

namespace gfx {

namespace rp {

enum class Protocol {
    PhongInstancingNT,
    PhongInstancing,
    ShadowMapGen,
    PhongInstancingShadowed,
    Null,
};

enum class DIType {
    Mesh,
    PID,
    PDD,
    PFD,
    Light,
    Material
};

template <class T>
concept PFUnified = requires {
    T::protocol;
    T::typeIdx;
    T::meshIdx;
    T::PIDIdx;
    T::PDDIdx;
    T::PFDIdx;
    T::lightIdx;
    T::materialIdx;
    typename T::PIDType;
    typename T::PDDType;
    typename T::PFDType;
    typename T::LightType;
    typename T::MaterialType;
    typename T::FPIDType;
    typename T::FPDDType;
    typename T::FPFDType;
    typename T::FLightType;
    typename T::FMaterialType;
};

template <class T>
concept PFUnifiedShader = requires (T& t){
    t.maxLightCnt();
    t.maxInstCnt();
    t.frameIdx();
    t.pid();
    t.pdd();
    t.pfd();
    t.lights();
};

struct Null {
    static constexpr Protocol protocol = Protocol::Null;
    static constexpr std::size_t typeIdx = 0u;
    static constexpr std::size_t meshIdx = 1u;
    static constexpr std::size_t PIDIdx = 2u;
    static constexpr std::size_t PDDIdx = 3u;
    static constexpr std::size_t PFDIdx = 4u;
    static constexpr std::size_t lightIdx = 5u;
    static constexpr std::size_t materialIdx = 6u;

    using PIDType = void;
    using PDDType = void;
    using PFDType = void;
    using LightType = void;
    using MaterialType = void;

    using FPIDType = void;
    using FPDDType = void;
    using FPFDType = void;
    using FLightType = void;
    using FMaterialType = void;
};

struct PhongInstancingNT {
    static constexpr Protocol protocol = Protocol::PhongInstancingNT;
    static constexpr std::size_t typeIdx = 0u;
    static constexpr std::size_t meshIdx = 1u;
    static constexpr std::size_t PIDIdx = 2u;
    static constexpr std::size_t PDDIdx = 3u;
    static constexpr std::size_t PFDIdx = 4u;
    static constexpr std::size_t lightIdx = 5u;
    static constexpr std::size_t materialIdx = 6u;

    using PIDType = d3d12::sr::BasicPID;
    using PDDType = d3d12::sr::PDDNTPhong;
    using PFDType = d3d12::sr::BasicPFD;
    using LightType = d3d12::sr::PhongLight;
    using MaterialType = d3d12::sr::PhongMaterialNT;

    using FPIDType = std::vector<PIDType>;
    using FPDDType = PDDType;
    using FPFDType = PFDType;
    using FLightType = std::vector<LightType>;
    using FMaterialType = MaterialType;
};

struct PhongInstancing {
    static constexpr Protocol protocol = Protocol::PhongInstancing;
    static constexpr std::size_t typeIdx = 0u;
    static constexpr std::size_t meshIdx = 1u;
    static constexpr std::size_t PIDIdx = 2u;
    static constexpr std::size_t PDDIdx = 3u;
    static constexpr std::size_t PFDIdx = 4u;
    static constexpr std::size_t lightIdx = 5u;
    static constexpr std::size_t materialIdx = 6u;

    // temporary place it here,
    // move it to d3d12 root related header file.
    static constexpr const char* DescRangeIDTex2D = "tex2D";

    using PIDType = d3d12::sr::BasicPID;
    using PDDType = d3d12::sr::PDDPhong;
    using PFDType = d3d12::sr::BasicPFD;
    using LightType = d3d12::sr::PhongLight;
    using MaterialType = d3d12::sr::PhongMaterial;

    using FPIDType = std::vector<PIDType>;
    using FPDDType = PDDType;
    using FPFDType = PFDType;
    using FLightType = std::vector<LightType>;
    using FMaterialType = MaterialType;
};

struct PhongInstancingShadowed {
    static constexpr Protocol protocol = Protocol::PhongInstancingShadowed;
    static constexpr std::size_t typeIdx = 0u;
    static constexpr std::size_t meshIdx = 1u;
    static constexpr std::size_t PIDIdx = 2u;
    static constexpr std::size_t PDDIdx = 3u;
    static constexpr std::size_t PFDIdx = 4u;
    static constexpr std::size_t lightIdx = 5u;
    static constexpr std::size_t materialIdx = 6u;

    using PIDType = d3d12::sr::BasicPID;
    using PDDType = d3d12::sr::PDDPhong;
    using PFDType = d3d12::sr::BasicPFDShadowed;
    using LightType = d3d12::sr::PhongLight;
    using MaterialType = d3d12::sr::PhongMaterial;

    using FPIDType = std::vector<PIDType>;
    using FPDDType = PDDType;
    using FPFDType = PFDType;
    using FLightType = std::vector<LightType>;
    using FMaterialType = MaterialType;
};

struct ShadowMapGen {
    static constexpr Protocol protocol = Protocol::ShadowMapGen;
    static constexpr std::size_t typeIdx = 0u;
    static constexpr std::size_t meshIdx = 1u;
    static constexpr std::size_t PIDIdx = 2u;
    static constexpr std::size_t PDDIdx = 3u;
    static constexpr std::size_t PFDIdx = 4u;

    // temporary place it here,
    // move it to d3d12 root related header file.
    static constexpr const char* DescRangeIDShadowTex = "shadowTex";
    static constexpr const char* DescRangeIDShadowDS = "shadowDS";

    using PIDType = d3d12::sr::BasicPID;
    using PDDType = d3d12::sr::PDDShadow;
    using PFDType = d3d12::sr::PFDShadow;

    using FPIDType = std::vector<PIDType>;
    using FPDDType = PDDType;
    using FPFDType = PFDType;
};

}   // namespace gfx::rp

}   // namespace gfx

#endif // !__RENDERPROTOCOL_HPP



