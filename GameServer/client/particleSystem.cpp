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

void ParticleSystem::emit(const EmitterConfig& config, int count) {
    if (!config.pClip || count <= 0) return;

    const mu::NVec3 axis(config.direction);

    for (int i = 0; i < count; ++i) {
        Particle* pSlot;
        if (activeCount_ < kMaxParticles) {
            pSlot = &pool_[activeCount_];
            ++activeCount_;
        } else {
            // pool full: round-robin 덮어쓰기 (기존 ring-buffer 동작과 동등)
            pSlot = &pool_[overwriteCursor_];
            overwriteCursor_ = (overwriteCursor_ + 1) % kMaxParticles;
        }
        Particle& p = *pSlot;

        const float    speed = randomFloat(config.speedMin, config.speedMax);
        const mu::Vec3 dir   = sampleConeDirection(axis, config.spread, rng_);

        mu::Vec3 spawnPos = config.position;
        if (config.shape == EmitterShape::Edge) {
            const float lenSq = mu::dot(config.edgeDir, config.edgeDir);
            if (lenSq > 1e-6f) {
                const mu::Vec3 edgeDirNorm = mu::Vec3(mu::normalize(config.edgeDir));
                spawnPos += edgeDirNorm * randomFloat(
                    -config.edgeLength * 0.5f, config.edgeLength * 0.5f);
            }
        }

        p.pos         = spawnPos;
        p.vel         = dir * speed;
        p.lifetime    = randomFloat(config.lifetimeMin, config.lifetimeMax);
        p.maxLifetime = p.lifetime;
        p.tintBegin   = config.tintBegin;
        p.tintEnd     = config.tintEnd;
        const float sizeMult = randomFloat(config.sizeMultiplierMin, config.sizeMultiplierMax);
        p.sizeBegin   = config.sizeBegin * sizeMult;
        p.sizeEnd     = config.sizeEnd   * sizeMult;
        p.drag        = config.drag;
        p.gravity     = config.gravity * randomFloat(config.gravityModifierMin, config.gravityModifierMax);
        p.rotation    = randomFloat(config.startRotationMin, config.startRotationMax);
        p.additive    = config.additiveBlend;
        p.anim.init(config.pClip);

        // 애니메이션 속도를 파티클 lifetime에 동기화:
        // Loop 타입 → lifetime 동안 정수 N번의 완전한 루프 사이클 재생
        // Once 타입 → lifetime 과 동시에 애니메이션 완료
        if (config.pClip->duration.count() > 0 && p.lifetime > 0.f) {
            const float lifetimeMs = p.lifetime * 1000.f;
            const float durationMs = static_cast<float>(config.pClip->duration.count());
            float speed = 1.f;
            if (config.pClip->type == SpriteAnimType::Once) {
                speed = durationMs / lifetimeMs;
            } else if (config.pClip->type == SpriteAnimType::Loop) {
                const float cycles = std::max(1.f, std::round(lifetimeMs / durationMs));
                speed = cycles * durationMs / lifetimeMs;
            }
            p.anim.setSpeed(speed);
        }

        p.anim.setAdditive(config.additiveBlend);
        p.anim.setRenderOrder(config.renderOrder);
        p.anim.setPos(spawnPos);
        p.anim.setRotation(p.rotation);
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

    int i = 0;
    while (i < activeCount_) {
        Particle& p = pool_[i];

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

        if (p.lifetime <= 0.f || p.anim.done()) {
            // swap-remove: 마지막 활성 파티클을 현재 슬롯으로 이동
            if (i != activeCount_ - 1)
                pool_[i] = std::move(pool_[activeCount_ - 1]);
            --activeCount_;
            // i 증가 안 함: 이동된 파티클을 다음 반복에서 처리
        } else {
            ++i;
        }
    }
}

void ParticleSystem::render(GFX& gfx) const {
    for (int i = 0; i < activeCount_; ++i)
        pool_[i].anim.render(gfx);
}
