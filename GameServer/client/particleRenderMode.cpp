#include "pch.hpp"
#include "particleRenderMode.hpp"
#include "particleSystem.hpp"

namespace {

mu::Mat4x4 buildParticleAngularRotation(const Particle& p) {
    return mu::rotateXH(mu::Radian{ p.angularAngle3D.x() })
         * mu::rotateYH(mu::Radian{ p.angularAngle3D.y() })
         * mu::rotateZH(mu::Radian{ p.angularAngle3D.z() });
}

float vecLength(mu::Vec3 v) {
    return std::sqrt(mu::dot(v, v));
}

}  // namespace

std::optional<ParticleBillboardGeometry> buildParticleBillboardGeometry(
    const ParticleRenderContext& ctx
) {
    const auto& rend = ctx.renderer;
    if (rend.mode != ps::RendererModule::Mode::Billboard &&
        rend.mode != ps::RendererModule::Mode::StretchedBillboard) {
        return std::nullopt;
    }

    float width = ctx.size;
    float height = ctx.size;
    mu::Vec4 stretchAxisAndMode = { 0.f, 0.f, 0.f, 0.f };

    if (rend.mode == ps::RendererModule::Mode::StretchedBillboard) {
        const float speed = vecLength(ctx.particle.motionVelocity);
        height = std::max(ctx.size, ctx.size * std::max(1.f, rend.lengthScale)
                                    + speed * rend.velocityScale);

        if (speed > 0.0001f) {
            const mu::Vec3 axis = ctx.particle.motionVelocity * (1.f / speed);
            stretchAxisAndMode = { axis.x(), axis.y(), axis.z(), 1.f };
        }
    }

    return ParticleBillboardGeometry{
        .world = mu::scaleH(mu::Vec3{ width, height, width })
               * mu::translate(ctx.particle.pos),
        .stretchAxisAndMode = stretchAxisAndMode,
        .rotation = ctx.particle.rotation,
    };
}

std::optional<ParticleMeshGeometry> buildParticleMeshGeometry(
    const ParticleRenderContext& ctx
) {
    const auto& rend = ctx.renderer;
    if (rend.mode != ps::RendererModule::Mode::Mesh ||
        !rend.pMesh || !rend.pSubMesh) {
        return std::nullopt;
    }

    return ParticleMeshGeometry{
        .world = mu::scaleH(mu::Vec3{ ctx.size, ctx.size, ctx.size })
               * buildParticleAngularRotation(ctx.particle)
               * ctx.particle.baseRotation
               * mu::translate(ctx.particle.pos),
        .pMesh = rend.pMesh,
        .pSubMesh = rend.pSubMesh,
    };
}

ParticleUVRect calcParticleTextureSheetUV(
    const ps::TextureSheetAnimationModule& tsa, float t
) {
    if (!tsa.enabled)
        return {};

    const int totalFrames = (tsa.animation == ps::TextureSheetAnimationModule::Animation::WholeSheet)
                            ? tsa.tilesX * tsa.tilesY
                            : tsa.tilesX;
    const int frameIndex = static_cast<int>(t * totalFrames * tsa.cycles)
                         % totalFrames + tsa.startFrame;
    const int col = frameIndex % tsa.tilesX;
    const int row = (tsa.animation == ps::TextureSheetAnimationModule::Animation::WholeSheet)
                    ? frameIndex / tsa.tilesX
                    : 0;

    return ParticleUVRect{
        .offset = { col / static_cast<float>(tsa.tilesX), row / static_cast<float>(tsa.tilesY) },
        .scale = { 1.f / tsa.tilesX, 1.f / tsa.tilesY },
    };
}
