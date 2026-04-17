#include "pch.hpp"
#include "meshParticleSystem.hpp"
#include "meshParticlePipeline.hpp"
#include "gfx.hpp"

float MeshParticleSystem::randomFloat(float lo, float hi) {
    return std::uniform_real_distribution<float>{ lo, hi }(rng_);
}

void MeshParticleSystem::emit(const MeshEmitterConfig& config, int count) {
    if (!config.pMesh || !config.pSubMesh || !config.pTex || count <= 0) return;

    for (int i = 0; i < count; ++i) {
        MeshParticle* pSlot;
        if (activeCount_ < kMaxParticles) {
            pSlot = &pool_[activeCount_];
            ++activeCount_;
        } else {
            pSlot = &pool_[overwriteCursor_];
            overwriteCursor_ = (overwriteCursor_ + 1) % kMaxParticles;
        }
        MeshParticle& p = *pSlot;

        p.pos               = config.position;
        p.lifetime          = randomFloat(config.lifetimeMin, config.lifetimeMax);
        p.maxLifetime       = p.lifetime;
        p.startColor        = config.startColor;
        p.colorOverLifetime = config.colorOverLifetime;
        p.sizeBegin         = config.sizeBegin;
        p.sizeEnd           = config.sizeEnd;
        p.rotation          = config.rotation;
        p.angularVelocity   = randomFloat(config.angularVelocityMin, config.angularVelocityMax);
        p.angle             = 0.f;
        p.pMesh             = config.pMesh;
        p.pSubMesh          = config.pSubMesh;
        p.pTex              = config.pTex;
        p.renderOrder       = config.renderOrder;
    }
}

void MeshParticleSystem::startContinuous(const MeshEmitterConfig& config) {
    continuousConfig_ = config;
    continuous_       = true;
    emitAccum_        = 0.f;
}

void MeshParticleSystem::stopContinuous() {
    continuous_ = false;
}

void MeshParticleSystem::update(Seconds dt) {
    const float dtf = dt.count();

    if (continuous_ && continuousConfig_.emitRate > 0.f) {
        emitAccum_ += continuousConfig_.emitRate * dtf;
        const int count = static_cast<int>(emitAccum_);
        emitAccum_ -= static_cast<float>(count);
        if (count > 0) emit(continuousConfig_, count);
    }

    int i = 0;
    while (i < activeCount_) {
        MeshParticle& p = pool_[i];
        p.lifetime -= dtf;
        p.angle    += p.angularVelocity * dtf;

        if (p.lifetime <= 0.f) {
            if (i != activeCount_ - 1)
                pool_[i] = std::move(pool_[activeCount_ - 1]);
            --activeCount_;
        } else {
            ++i;
        }
    }
}

void MeshParticleSystem::render(GFX& gfx) const {
    for (int i = 0; i < activeCount_; ++i) {
        const MeshParticle& p = pool_[i];

        const float t    = 1.f - p.lifetime / p.maxLifetime;
        const float size = std::lerp(p.sizeBegin, p.sizeEnd, t);
        const mu::Vec4 tint = p.startColor * p.colorOverLifetime.evaluate(t);

        // Rotation over Lifetime: spin around mesh local Z axis (face normal).
        // Applied BEFORE p.rotation so the axis is in local space.
        // After the art corrections in p.rotation, local Z ≈ world Y,
        // so this produces a Y-axis-like sweeping rotation in world space.
        const auto rotDelta = mu::rotateZH(mu::Radian{ p.angle });

        // world = Scale * RotDelta(local) * BaseRot * Translation
        const auto scaleM = mu::scaleH(mu::Vec3{ size, size, size });
        const auto transM = mu::translate(p.pos);
        const auto world  = scaleM * rotDelta * p.rotation * transM;

        gfx.addDrawEvent(MeshParticlePipeline::DrawEvent{
            .world      = world,
            .pMesh      = p.pMesh,
            .pSubMesh   = p.pSubMesh,
            .pTex       = p.pTex,
            .tint       = tint,
            .renderOrder = p.renderOrder,
        });
    }
}
