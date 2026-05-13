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

mu::Vec3 rotateVector(mu::Vec3 v, mu::Mat4x4 rotation) {
    return mu::Vec3(mu::Vec4(v.x(), v.y(), v.z(), 0.f) * rotation);
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

    float width = ctx.size3D.x();
    float height = ctx.size3D.y();
    mu::Vec4 stretchAxisAndMode = { 0.f, 0.f, 0.f, 0.f };

    if (rend.mode == ps::RendererModule::Mode::StretchedBillboard) {
        constexpr float kMinStableStretchSpeed = 0.05f;
        const float speed = vecLength(ctx.particle.motionVelocity);
        height = ctx.size3D.y() * rend.lengthScale + speed * rend.velocityScale;

        if (speed > kMinStableStretchSpeed) {
            const mu::Vec3 axis = ctx.particle.motionVelocity * (1.f / speed);
            stretchAxisAndMode = { axis.x(), axis.y(), axis.z(), 1.f };
        } else {
            const mu::Vec3 axis = rotateVector({ 0.f, 1.f, 0.f }, ctx.particle.emitterRotation);
            const float axisLen = vecLength(axis);
            if (axisLen > 0.0001f) {
                const mu::Vec3 unitAxis = axis * (1.f / axisLen);
                stretchAxisAndMode = { unitAxis.x(), unitAxis.y(), unitAxis.z(), 1.f };
            }
        }
    }

    return ParticleBillboardGeometry{
        .world = mu::scaleH(mu::Vec3{ width, height, ctx.size3D.z() })
               * mu::translate(ctx.particle.pos),
        .rotation3D = buildParticleAngularRotation(ctx.particle)
                    * ctx.particle.billboardRotation3D,
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
        .world = mu::scaleH(ctx.particle.transformScale * ctx.size3D)
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
