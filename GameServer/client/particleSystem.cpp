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

void ParticleSystem::emit(const EmitterConfig& config, int count) {
    if (!config.pClip || count <= 0) return;

    std::uniform_real_distribution<float> speedDist(config.speedMin, config.speedMax);
    std::uniform_real_distribution<float> lifeDist(config.lifetimeMin, config.lifetimeMax);
    std::uniform_real_distribution<float> rotDist(config.startRotationMin, config.startRotationMax);

    const mu::NVec3 axis(config.direction);

    for (int i = 0; i < count; ++i) {
        Particle& p        = pool_[cursor_];
        cursor_            = (cursor_ + 1) % kMaxParticles;
        const float  speed = speedDist(rng_);
        const mu::Vec3 dir = sampleConeDirection(axis, config.spread, rng_);
		// Spawn position starts at emitter position.
        mu::Vec3 spawnPos = config.position;
		// If the emitter shape is an edge, offset the spawn position along the edge direction.
        if (config.shape == EmitterShape::Edge) {
            const float lenSq = mu::dot(config.edgeDir, config.edgeDir);
            if (lenSq > 1e-6f) {
                const mu::Vec3 edgeDirNorm = mu::Vec3(mu::normalize(config.edgeDir));
                std::uniform_real_distribution<float> edgeDist(
                    -config.edgeLength * 0.5f, config.edgeLength * 0.5f);
                spawnPos += edgeDirNorm * edgeDist(rng_);
            }
        }
        p.pos         = spawnPos;
        p.vel         = dir * speed;
        p.lifetime    = lifeDist(rng_);
        p.maxLifetime = p.lifetime;
        p.tintBegin   = config.tintBegin;
        p.tintEnd     = config.tintEnd;
        p.sizeBegin   = config.sizeBegin;
        p.sizeEnd     = config.sizeEnd;
        p.drag        = config.drag;
        p.gravity     = config.gravity;
        p.rotation    = rotDist(rng_);
        p.additive    = config.additiveBlend;
        p.anim.init(config.pClip);
        p.anim.setAdditive(config.additiveBlend);
        p.anim.setPos(spawnPos);
        p.anim.setRotation(p.rotation);
        p.active      = true;
    }
}

void ParticleSystem::startContinuous(const EmitterConfig& config) {
    continuousConfig_ = config;
    continuous_       = true;
    emitAccum_        = 0.f;
}

void ParticleSystem::stopContinuous() {
    continuous_ = false;
}

void ParticleSystem::update(Seconds dt) {
    const float dtf = dt.count();

    if (continuous_ && continuousConfig_.emitRate > 0.f) {
        emitAccum_ += continuousConfig_.emitRate * dtf;
        const int count = static_cast<int>(emitAccum_);
        emitAccum_ -= static_cast<float>(count);
        if (count > 0) emit(continuousConfig_, count);
    }

    const Milliseconds dtMs = std::chrono::duration_cast<Milliseconds>(dt);
    for (auto& p : pool_) {
        if (!p.active) continue;
        p.vel      *= std::max(0.f, 1.f - p.drag * dtf);
        p.vel      += p.gravity * dtf;
        p.pos      += p.vel * dtf;
        p.lifetime -= dtf;

        const float    t    = 1.f - p.lifetime / p.maxLifetime;
        const float    size = std::lerp(p.sizeBegin, p.sizeEnd, t);
        const mu::Vec3 tint = mu::lerp(p.tintBegin, p.tintEnd, t);

        p.anim.setScale(mu::Vec2(size, size));
        p.anim.setTint(tint);
        p.anim.update(dtMs);
        p.anim.setPos(p.pos);
        if (p.lifetime <= 0.f || p.anim.done()) p.active = false;
    }
}

void ParticleSystem::render(GFX& gfx) {
    for (auto& p : pool_) {
        if (!p.active) continue;
        p.anim.setAdditive(p.additive);
        p.anim.render(gfx);
    }
}
