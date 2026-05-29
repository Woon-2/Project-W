#ifndef __particleRenderMode_HPP
#define __particleRenderMode_HPP

#include "particleRenderSubmit.hpp"

#include <optional>

struct ParticleBillboardGeometry {
    mu::Mat4x4 world;
    mu::Mat4x4 rotation3D;
    mu::Vec4 stretchAxisAndMode = { 0.f, 0.f, 0.f, 0.f };
    float rotation = 0.f;
};

struct ParticleMeshGeometry {
    mu::Mat4x4 world;
    const Mesh* pMesh = nullptr;
    const SubMesh* pSubMesh = nullptr;
};

struct ParticleUVRect {
    mu::Vec2 offset = { 0.f, 0.f };
    mu::Vec2 scale = { 1.f, 1.f };
};

std::optional<ParticleBillboardGeometry> buildParticleBillboardGeometry(
    const ParticleRenderContext& ctx
);

std::optional<ParticleMeshGeometry> buildParticleMeshGeometry(
    const ParticleRenderContext& ctx
);

ParticleUVRect calcParticleTextureSheetUV(
    const ps::TextureSheetAnimationModule& tsa, float t
);

#endif  // __particleRenderMode_HPP
