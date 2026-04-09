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
        if (activeCount_ < maxParticles_) {
            pSlot = &pool_[activeCount_];
            ++activeCount_;
        } else {
            // pool full: round-robin 덮어쓰기 (기존 ring-buffer 동작과 동등)
            pSlot = &pool_[overwriteCursor_];
            overwriteCursor_ = (overwriteCursor_ + 1) % maxParticles_;
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
        p.startColor        = config.startColor;
        p.colorOverLifetime = config.colorOverLifetime;
        const float sizeMult = randomFloat(config.sizeMultiplierMin, config.sizeMultiplierMax);
        p.sizeBegin   = config.sizeBegin * sizeMult;
        p.sizeEnd     = config.sizeEnd   * sizeMult;
        p.drag        = config.drag;
        p.gravity     = config.gravity * randomFloat(config.gravityModifierMin, config.gravityModifierMax);
        p.rotation    = randomFloat(config.startRotationMin, config.startRotationMax);
        p.additive    = config.additiveBlend;
        p.anim.init(config.pClip);
        p.hasAnim = true;

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
    continuous_    = false;
    continuousNew_ = false;
}

void ParticleSystem::update(Seconds dt) {
    const float dtf = dt.count();

    if (continuous_ && continuousConfig_.emitRate > 0.f) {
        emitAccum_ += continuousConfig_.emitRate * dtf;
        const int count = static_cast<int>(emitAccum_);
        emitAccum_ -= static_cast<float>(count);
        if (count > 0) emit(continuousConfig_, count);
    }

    // 새 path: config_ 기반 연속 방출
    if (continuousNew_ && config_.emission.emitRate > 0.f) {
        emitAccumNew_ += config_.emission.emitRate * dtf;
        const int n = static_cast<int>(emitAccumNew_);
        emitAccumNew_ -= static_cast<float>(n);
        if (n > 0) emit(n);
    }

    const Milliseconds dtMs = std::chrono::duration_cast<Milliseconds>(dt);

    int i = 0;
    while (i < activeCount_) {
        Particle& p = pool_[i];

        p.vel      *= std::max(0.f, 1.f - p.drag * dtf);
        p.vel      += p.gravity * dtf;
        p.pos      += p.vel * dtf;
        p.lifetime -= dtf;

        const float    t         = 1.f - p.lifetime / p.maxLifetime;
        const float    size      = std::lerp(p.sizeBegin, p.sizeEnd, t);
        const mu::Vec4 finalColor = p.startColor * p.colorOverLifetime.evaluate(t);

        // 기존 billboard 렌더 경로 (Phase 2에서 제거 예정)
        if (p.hasAnim) {
            p.anim.setScale(mu::Vec2(size, size));
            p.anim.setTint(finalColor);
            p.anim.update(dtMs);
            p.anim.setPos(p.pos);
        }

        // mesh 회전 누적 (hasAnim이 false인 mesh particle에만 적용)
        if (!p.hasAnim && p.angularVelocity != 0.f)
            p.angularAngle += p.angularVelocity * dtf;

        // semantic payload 갱신 (renderer backend가 Phase 2 이후 소비)
        updatePayload(p, t);

        if (p.lifetime <= 0.f || (p.hasAnim && p.anim.done())) {
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
    for (int i = 0; i < activeCount_; ++i) {
        if (pool_[i].hasAnim)
            pool_[i].anim.render(gfx);
        // Mesh mode render path added in Phase 2 (IParticleRenderer::flush)
    }
}

// ── New module-based API implementation ─────────────────────────────────────

void ParticleSystem::init(const ParticleSystemConfig& config, int maxParticles) {
    config_       = config;
    maxParticles_ = maxParticles;
    pool_.resize(maxParticles_);
    activeCount_     = 0;
    overwriteCursor_ = 0;
    emitAccumNew_    = 0.f;
    continuousNew_   = false;
}

void ParticleSystem::emit(int count) {
    for (int i = 0; i < count; ++i)
        spawnParticle();
}

void ParticleSystem::startContinuous() {
    continuousNew_ = true;
    emitAccumNew_  = 0.f;
}

void ParticleSystem::startContinuous(const ParticleSystemConfig& config) {
    init(config);
    startContinuous();
}

// ── Shape sampling ───────────────────────────────────────────────────────────

mu::Vec3 ParticleSystem::sampleShapeOrigin() {
    const ShapeModule& s = config_.shape;
    switch (s.type) {
    case ShapeModule::Type::Point:
        return s.position;

    case ShapeModule::Type::Edge: {
        const float lenSq = mu::dot(s.edgeDir, s.edgeDir);
        if (lenSq < 1e-6f) return s.position;
        const mu::Vec3 dir = mu::Vec3(mu::normalize(s.edgeDir));
        return s.position + dir * randomFloat(-s.edgeLength * 0.5f, s.edgeLength * 0.5f);
    }

    case ShapeModule::Type::Cone:
        if (s.coneRadius > 0.f) {
            // random point on base disc; disc is perpendicular to the up axis by default
            const float angle = randomFloat(0.f, 2.f * 3.14159265f);
            const float r     = s.coneRadius * std::sqrt(randomFloat(0.f, 1.f));
            return s.position + mu::Vec3{ r * std::cos(angle), 0.f, r * std::sin(angle) };
        }
        return s.position;  // apex emit

    case ShapeModule::Type::Sphere: {
        const float theta = 2.f * 3.14159265f * randomFloat(0.f, 1.f);
        const float phi   = std::acos(1.f - 2.f * randomFloat(0.f, 1.f));
        const float sp    = std::sin(phi);
        return s.position + mu::Vec3{
            s.sphereRadius * sp * std::cos(theta),
            s.sphereRadius * std::cos(phi),
            s.sphereRadius * sp * std::sin(theta) };
    }

    case ShapeModule::Type::Box:
        return s.position + mu::Vec3{
            randomFloat(-s.boxSize.x() * 0.5f,  s.boxSize.x() * 0.5f),
            randomFloat(-s.boxSize.y() * 0.5f,  s.boxSize.y() * 0.5f),
            randomFloat(-s.boxSize.z() * 0.5f,  s.boxSize.z() * 0.5f) };
    }
    return s.position;
}

mu::Vec3 ParticleSystem::sampleShapeDirection(const mu::Vec3& origin) {
    const ShapeModule& s = config_.shape;
    switch (s.type) {
    case ShapeModule::Type::Point:
        // Point: emit along the configured direction axis with no spread
        return s.direction;

    case ShapeModule::Type::Edge:
        // Emit upward (perpendicular to edge) along the configured direction
        return s.direction;

    case ShapeModule::Type::Cone:
        return sampleConeDirection(mu::NVec3(s.direction), s.coneAngle, rng_);

    case ShapeModule::Type::Sphere:
    case ShapeModule::Type::Box: {
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
    p.drag        = config_.main.drag;
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

    const bool additive = (config_.renderer.material.blendMode ==
                           MaterialDescriptor::BlendMode::Additive);
    p.additive = additive;

    if (config_.renderer.mode == RendererModule::Mode::Billboard &&
        config_.renderer.pClip != nullptr)
    {
        p.anim.init(config_.renderer.pClip);
        p.hasAnim = true;
        p.angularVelocity = 0.f;
        p.angularAngle    = 0.f;

        // 애니메이션 속도를 파티클 lifetime에 동기화 (기존 로직과 동일)
        if (config_.renderer.pClip->duration.count() > 0 && p.lifetime > 0.f) {
            const float lifetimeMs = p.lifetime * 1000.f;
            const float durationMs = static_cast<float>(config_.renderer.pClip->duration.count());
            float animSpeed = 1.f;
            if (config_.renderer.pClip->type == SpriteAnimType::Once) {
                animSpeed = durationMs / lifetimeMs;
            } else if (config_.renderer.pClip->type == SpriteAnimType::Loop) {
                const float cycles = std::max(1.f, std::round(lifetimeMs / durationMs));
                animSpeed = cycles * durationMs / lifetimeMs;
            }
            p.anim.setSpeed(animSpeed);
        }
        p.anim.setAdditive(additive);
        p.anim.setRenderOrder(config_.renderer.renderOrder);
        p.anim.setPos(origin);
        p.anim.setRotation(p.rotation);
    } else {
        // Mesh particle or billboard without clip
        p.hasAnim         = false;
        p.angularVelocity = randomFloat(config_.renderer.angularVelocityMin,
                                        config_.renderer.angularVelocityMax);
        p.angularAngle    = 0.f;
        // baseRotation: left as default-constructed; Phase 2 will wire up
    }

    updatePayload(p, 0.f);
}

// ── Payload update ───────────────────────────────────────────────────────────

void ParticleSystem::updatePayload(Particle& p, float t) const {
    p.payload.color = p.startColor * p.colorOverLifetime.evaluate(t);

    // uvFrame: Phase 3 will read the current sprite animation frame here.
    // Until then, full UV (no sprite sheet offset applied via payload).
    p.payload.uvFrame = { 0.f, 0.f, 1.f, 1.f };

    // uv1: no source in Phase 1; zeroed
    p.payload.uv1 = mu::Vec2(0.f, 0.f);

    if (config_.customData.enabled) {
        p.payload.custom0 = config_.customData.custom0Constant;
        p.payload.custom1 = config_.customData.custom1Constant;
    } else {
        p.payload.custom0 = mu::Vec2(0.f, 0.f);
        p.payload.custom1 = mu::Vec2(0.f, 0.f);
    }
}
