#include "pch.hpp"
#include "particleSystem.hpp"
#include "gfx.hpp"


// Samples a random direction within a cone of half-angle `spread` around `axis`.
// Uses rejection-free spherical cap sampling.
static mu::Vec3 sampleConeDirection(mu::NVec3 axis, float spread, std::mt19937& rng) {
    std::uniform_real_distribution<float> u01(0.f, 1.f);
    const float cosSpread = std::cos(spread);

    // uniform sample on spherical cap: cos(theta) in [cosSpread, 1]
    const float cosTheta = cosSpread + (1.f - cosSpread) * u01(rng);
    const float sinTheta = std::sqrt(1.f - cosTheta * cosTheta);
    const float phi      = 2.f * 3.14159265f * u01(rng);

    // build orthonormal frame around axis
    const mu::Vec3 axisV = mu::Vec3(axis);
    const mu::Vec3 up    = (std::abs(axisV.x()) < 0.9f)
                           ? mu::Vec3{1.f, 0.f, 0.f}
                           : mu::Vec3{0.f, 1.f, 0.f};
    const mu::NVec3 tangent   = mu::cross(axis, mu::NVec3(up));
    const mu::Vec3  bitangent = mu::cross(axis, tangent);

    return sinTheta * std::cos(phi) * mu::Vec3(tangent)
         + sinTheta * std::sin(phi) * bitangent
         + cosTheta * axisV;
}

float ParticleSystem::randomFloat(float lo, float hi) {
    return std::uniform_real_distribution<float>{lo, hi}(rng_);
}

void ParticleSystem::stopContinuous() {
    continuousNew_ = false;
}

void ParticleSystem::update(Seconds dt) {
    const float scaledDtf = dt.count() * config_.main.simulationSpeed;

    // Start Delay -> Emission -> Duration / Looping
    if (continuousNew_) {
        if (delayRemaining_ > 0.f) {
            delayRemaining_ -= scaledDtf;
        } else {
            if (config_.emission.emitRate > 0.f) {
                emitAccumNew_ += config_.emission.emitRate * scaledDtf;
                const int n = static_cast<int>(emitAccumNew_);
                emitAccumNew_ -= static_cast<float>(n);
                if (n > 0) emit(n);
            }

            if (config_.main.duration > 0.f) {
                systemTime_ += scaledDtf;
                if (systemTime_ >= config_.main.duration) {
                    if (config_.main.looping) {
                        systemTime_ -= config_.main.duration;
                    } else {
                        continuousNew_ = false;
                        systemTime_    = 0.f;
                    }
                }
            }
        }
    }

    int i = 0;
    while (i < activeCount_) {
        Particle& p = pool_[i];

        p.vel      *= std::max(0.f, 1.f - p.drag * scaledDtf);
        p.vel      += p.gravity * scaledDtf;
        p.pos      += p.vel * scaledDtf;
        p.lifetime -= scaledDtf;

        if (p.angularVelocity != 0.f)
            p.angularAngle += p.angularVelocity * scaledDtf;

        if (p.lifetime <= 0.f) {
            if (i != activeCount_ - 1)
                pool_[i] = std::move(pool_[activeCount_ - 1]);
            --activeCount_;
        } else {
            ++i;
        }
    }
}

void ParticleSystem::render(GFX& gfx) const {
    const auto& rend = config_.renderer;
    const auto& tsa  = config_.textureSheetAnimation;

    for (int i = 0; i < activeCount_; ++i) {
        const Particle& p    = pool_[i];
        const float    t     = 1.f - p.lifetime / p.maxLifetime;
        const float    size  = std::lerp(p.sizeBegin, p.sizeEnd, t);
        const mu::Vec4 tint  = p.startColor * p.colorOverLifetime.evaluate(t);

        std::visit([&](const auto& mat) {
            using T = std::decay_t<decltype(mat)>;

            if constexpr (std::is_same_v<T, ps::MatUnlit>) {
                if (!mat.mainTex) return;

                // ── Billboard path ────────────────────────────────────────
                if (rend.mode == ps::RendererModule::Mode::Billboard) {
                    mu::Vec2 uvOffset = { 0.f, 0.f };
                    mu::Vec2 uvScale  = { 1.f, 1.f };
                    if (tsa.enabled) {
                        const int totalFrames = (tsa.animation == ps::TextureSheetAnimationModule::Animation::WholeSheet)
                                                ? tsa.tilesX * tsa.tilesY
                                                : tsa.tilesX;
                        const int frameIndex  = static_cast<int>(t * totalFrames * tsa.cycles)
                                                % totalFrames + tsa.startFrame;
                        const int col = frameIndex % tsa.tilesX;
                        const int row = (tsa.animation == ps::TextureSheetAnimationModule::Animation::WholeSheet)
                                        ? frameIndex / tsa.tilesX
                                        : 0;
                        uvOffset = { col / static_cast<float>(tsa.tilesX),
                                     row / static_cast<float>(tsa.tilesY) };
                        uvScale  = { 1.f / tsa.tilesX, 1.f / tsa.tilesY };
                    }
                    gfx.addDrawEvent(BillboardPipeline::DrawEvent{
                        .world       = mu::scaleH(mu::Vec3{ size, size, size })
                                     * mu::translate(p.pos),
                        .pTex        = mat.mainTex,
                        .uvOffset    = uvOffset,
                        .uvScale     = uvScale,
                        .tint        = tint,
                        .additive    = mat.additive,
                        .rotation    = p.rotation,
                        .renderOrder = rend.renderOrder,
                    });
                }
                // ── Mesh path ─────────────────────────────────────────────
                else if (rend.mode == ps::RendererModule::Mode::Mesh &&
                         rend.pMesh && rend.pSubMesh)
                {
                    const auto world = mu::scaleH(mu::Vec3{ size, size, size })
                                     * mu::rotateZH(mu::Radian{ p.angularAngle })
                                     * p.baseRotation
                                     * mu::translate(p.pos);
                    gfx.addDrawEvent(MeshParticlePipeline::DrawEvent{
                        .world       = world,
                        .pMesh       = rend.pMesh,
                        .pSubMesh    = rend.pSubMesh,
                        .pTex        = mat.mainTex,
                        .tint        = tint,
                        .renderOrder = rend.renderOrder,
                    });
                }
            }
            else if constexpr (std::is_same_v<T, ps::MatSwordSlash>) {
                // SwordSlashPipeline -- Step 4 (not yet implemented)
                (void)mat;
            }
        }, rend.mat);
    }
}

// ── New module-based API implementation ─────────────────────────────────────

void ParticleSystem::init(const ps::ParticleSystemConfig& config, int maxParticles) {
    config_       = config;
    maxParticles_ = maxParticles;
    pool_.resize(maxParticles_);
    activeCount_     = 0;
    overwriteCursor_ = 0;
    emitAccumNew_    = 0.f;
    continuousNew_   = false;
    systemTime_      = 0.f;
    delayRemaining_  = 0.f;
}

void ParticleSystem::emit(int count) {
    for (int i = 0; i < count; ++i)
        spawnParticle();
}

void ParticleSystem::startContinuous() {
    continuousNew_  = true;
    emitAccumNew_   = 0.f;
    systemTime_     = 0.f;
    delayRemaining_ = config_.main.startDelay;
}

void ParticleSystem::startContinuous(const ps::ParticleSystemConfig& config) {
    init(config);
    startContinuous();
}

// ── Shape sampling ───────────────────────────────────────────────────────────

mu::Vec3 ParticleSystem::sampleShapeOrigin() {
    const ps::ShapeModule& s = config_.shape;
    switch (s.type) {
    case ps::ShapeModule::Type::Point:
        return s.position;

    case ps::ShapeModule::Type::Edge: {
        const float lenSq = mu::dot(s.edgeDir, s.edgeDir);
        if (lenSq < 1e-6f) return s.position;
        const mu::Vec3 dir = mu::Vec3(mu::normalize(s.edgeDir));
        return s.position + dir * randomFloat(-s.edgeLength * 0.5f, s.edgeLength * 0.5f);
    }

    case ps::ShapeModule::Type::Cone:
        if (s.coneRadius > 0.f) {
            // random point on base disc; disc is perpendicular to the up axis by default
            const float angle = randomFloat(0.f, 2.f * 3.14159265f);
            const float r     = s.coneRadius * std::sqrt(randomFloat(0.f, 1.f));
            return s.position + mu::Vec3{ r * std::cos(angle), 0.f, r * std::sin(angle) };
        }
        return s.position;  // apex emit

    case ps::ShapeModule::Type::Sphere: {
        const float theta = 2.f * 3.14159265f * randomFloat(0.f, 1.f);
        const float phi   = std::acos(1.f - 2.f * randomFloat(0.f, 1.f));
        const float sp    = std::sin(phi);
        return s.position + mu::Vec3{
            s.sphereRadius * sp * std::cos(theta),
            s.sphereRadius * std::cos(phi),
            s.sphereRadius * sp * std::sin(theta) };
    }

    case ps::ShapeModule::Type::Box:
        return s.position + mu::Vec3{
            randomFloat(-s.boxSize.x() * 0.5f,  s.boxSize.x() * 0.5f),
            randomFloat(-s.boxSize.y() * 0.5f,  s.boxSize.y() * 0.5f),
            randomFloat(-s.boxSize.z() * 0.5f,  s.boxSize.z() * 0.5f) };
    }
    return s.position;
}

mu::Vec3 ParticleSystem::sampleShapeDirection(const mu::Vec3& origin) {
    const ps::ShapeModule& s = config_.shape;
    switch (s.type) {
    case ps::ShapeModule::Type::Point:
        // Point: emit along the configured direction axis with no spread
        return s.direction;

    case ps::ShapeModule::Type::Edge:
        // Emit upward (perpendicular to edge) along the configured direction
        return s.direction;

    case ps::ShapeModule::Type::Cone:
        return sampleConeDirection(mu::NVec3(s.direction), s.coneAngle, rng_);

    case ps::ShapeModule::Type::Sphere:
    case ps::ShapeModule::Type::Box: {
        // Outward from shape centre
        const mu::Vec3 outward = origin - s.position;
        const float    len     = std::sqrt(mu::dot(outward, outward));
        return (len > 1e-6f) ? outward * (1.f / len) : s.direction;
    }
    }
    return s.direction;
}

// ── Particle spawn ───────────────────────────────────────────────────────────

void ParticleSystem::spawnParticle() {
    Particle* pSlot;
    if (activeCount_ < maxParticles_) {
        pSlot = &pool_[activeCount_++];
    } else {
        pSlot = &pool_[overwriteCursor_];
        overwriteCursor_ = (overwriteCursor_ + 1) % maxParticles_;
    }
    Particle& p = *pSlot;

    const mu::Vec3 origin = sampleShapeOrigin();
    const mu::Vec3 dir    = sampleShapeDirection(origin);
    const float    speed  = randomFloat(config_.main.speedMin, config_.main.speedMax);

    p.pos         = origin;
    p.vel         = dir * speed;
    p.lifetime    = randomFloat(config_.main.lifetimeMin, config_.main.lifetimeMax);
    p.maxLifetime = p.lifetime;
    p.startColor  = config_.main.startColor;
    p.drag        = config_.velocityOverLifetime.enabled ? config_.velocityOverLifetime.drag : 0.f;
    p.gravity     = config_.main.gravity *
                    randomFloat(config_.main.gravityModifierMin, config_.main.gravityModifierMax);
    p.rotation    = randomFloat(config_.main.startRotationMin, config_.main.startRotationMax);

    if (config_.colorOverLifetime.enabled)
        p.colorOverLifetime = config_.colorOverLifetime.gradient;
    else
        p.colorOverLifetime = ColorGradient::constant({ 1.f, 1.f, 1.f, 1.f });

    const float sizeMult = randomFloat(config_.main.startSizeMin, config_.main.startSizeMax);
    if (config_.sizeOverLifetime.enabled) {
        p.sizeBegin = config_.sizeOverLifetime.sizeBegin * sizeMult;
        p.sizeEnd   = config_.sizeOverLifetime.sizeEnd   * sizeMult;
    } else {
        p.sizeBegin = p.sizeEnd = sizeMult;
    }

    p.angularVelocity = config_.rotationOverLifetime.enabled
                        ? randomFloat(config_.rotationOverLifetime.angularVelocityMin,
                                      config_.rotationOverLifetime.angularVelocityMax)
                        : 0.f;
    p.angularAngle    = 0.f;
    p.baseRotation    = config_.main.startRotation3D;
}
