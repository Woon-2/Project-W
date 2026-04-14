#include "pch.hpp"
#include "particleSystem.hpp"
#include "particleRenderSubmit.hpp"


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

float ps::FloatCurve::evaluate(float t) const {
    if (keys.empty())
        return 0.f;

    if (t <= keys.front().t)
        return keys.front().value;

    if (t >= keys.back().t)
        return keys.back().value;

    for (std::size_t i = 1; i < keys.size(); ++i) {
        const auto& k0 = keys[i - 1];
        const auto& k1 = keys[i];
        if (t > k1.t)
            continue;

        const float dt = k1.t - k0.t;
        if (dt <= 0.f)
            return k1.value;

        const float u = (t - k0.t) / dt;
        const float u2 = u * u;
        const float u3 = u2 * u;
        const float h00 = 2.f * u3 - 3.f * u2 + 1.f;
        const float h10 = u3 - 2.f * u2 + u;
        const float h01 = -2.f * u3 + 3.f * u2;
        const float h11 = u3 - u2;

        return h00 * k0.value
             + h10 * k0.outTangent * dt
             + h01 * k1.value
             + h11 * k1.inTangent * dt;
    }

    return keys.back().value;
}

float ps::MinMaxCurveChannel::evaluate(float t, float random01) const {
    switch (mode) {
    case Mode::Constant:
        return constant;

    case Mode::TwoConstants:
        return std::lerp(constantMin, constantMax, std::clamp(random01, 0.f, 1.f));

    case Mode::Curve:
        return curve.evaluate(t) * curveMultiplier;

    case Mode::TwoCurves:
        return std::lerp(curveMin.evaluate(t), curveMax.evaluate(t),
                         std::clamp(random01, 0.f, 1.f)) * curveMultiplier;
    }

    return constant;
}

float ps::CustomDataChannel::evaluate(float t, float random01) const {
    switch (mode) {
    case Mode::Constant:
        return constant;

    case Mode::TwoConstants:
        return std::lerp(constantMin, constantMax, std::clamp(random01, 0.f, 1.f));

    case Mode::Curve:
        return curve.evaluate(t) * curveMultiplier;

    case Mode::TwoCurves:
        return std::lerp(curveMin.evaluate(t), curveMax.evaluate(t),
                         std::clamp(random01, 0.f, 1.f)) * curveMultiplier;
    }

    return constant;
}

static mu::Vec2 evaluateCustomDataStream(
    const ps::CustomDataStream& stream, float t, mu::Vec2 random01
) {
    return {
        stream.x.evaluate(t, random01.x()),
        stream.y.evaluate(t, random01.y())
    };
}

static mu::Mat4x4 buildEulerRotation(mu::Vec3 eulerRadians) {
    return mu::rotateXH(mu::Radian{ eulerRadians.x() })
         * mu::rotateYH(mu::Radian{ eulerRadians.y() })
         * mu::rotateZH(mu::Radian{ eulerRadians.z() });
}

static mu::Mat4x4 buildShapeRotation(const ps::ShapeModule& s) {
    return buildEulerRotation(s.rotation) * s.orientation;
}

static mu::Vec3 rotateVector(mu::Vec3 v, mu::Mat4x4 rotation) {
    return mu::Vec3(mu::Vec4(v.x(), v.y(), v.z(), 0.f) * rotation);
}

static mu::Vec3 rotateShapeVector(mu::Vec3 v, const ps::ShapeModule& s) {
    return rotateVector(v, buildShapeRotation(s));
}

static mu::Vec3 rotateShapeDirection(mu::Vec3 v, const ps::ShapeModule& s) {
    const float lenSq = mu::dot(v, v);
    if (lenSq < 1e-6f)
        return v;
    return mu::Vec3(mu::normalize(rotateShapeVector(v, s)));
}

static mu::Vec3 rotateDirection(mu::Vec3 v, mu::Mat4x4 rotation) {
    const float lenSq = mu::dot(v, v);
    if (lenSq < 1e-6f)
        return v;
    return mu::Vec3(mu::normalize(rotateVector(v, rotation)));
}

static mu::Vec3 calcVelocityOverLifetime(
    const ps::ParticleSystemConfig& cfg, const Particle& p
) {
    const auto& vol = cfg.velocityOverLifetime;
    if (!vol.enabled)
        return { 0.f, 0.f, 0.f };

    const mu::Vec3 radialOffset = p.pos - p.emitterPosition;
    const float radius = std::sqrt(mu::dot(radialOffset, radialOffset));
    const mu::Vec3 radialDir = (radius > 1e-6f)
                             ? radialOffset * (1.f / radius)
                             : rotateDirection(cfg.shape.direction, p.emitterRotation);

    const mu::Vec3 localLinear = vol.inWorldSpace
                               ? vol.linear
                               : rotateVector(vol.linear, p.emitterRotation);

    mu::Vec3 velocity = localLinear + radialDir * vol.radial;

    auto addOrbitalVelocity = [&](mu::Vec3 localAxis, float angularSpeed) {
        if (std::abs(angularSpeed) <= 1e-6f || radius <= 1e-6f)
            return;

        const mu::Vec3 axis = vol.inWorldSpace
                            ? localAxis
                            : rotateDirection(localAxis, p.emitterRotation);
        const mu::Vec3 tangentRaw = mu::cross(axis, radialDir);
        const float tangentLenSq = mu::dot(tangentRaw, tangentRaw);
        if (tangentLenSq > 1e-6f) {
            const mu::Vec3 tangent = tangentRaw * (1.f / std::sqrt(tangentLenSq));
            velocity += tangent * (angularSpeed * radius);
        }
    };

    addOrbitalVelocity({ 1.f, 0.f, 0.f }, vol.orbital.x());
    addOrbitalVelocity({ 0.f, 1.f, 0.f }, vol.orbital.y());
    addOrbitalVelocity({ 0.f, 0.f, 1.f }, vol.orbital.z());

    return velocity * vol.speedModifier;
}

float ParticleSystem::randomFloat(float lo, float hi) {
    return std::uniform_real_distribution<float>{lo, hi}(rng_);
}

void ParticleSystem::emitScheduledBursts(float prevTime, float currTime) {
    if (!config_.emission.enabled || config_.emission.bursts.empty())
        return;

    if (burstNextCycle_.size() != config_.emission.bursts.size())
        burstNextCycle_.assign(config_.emission.bursts.size(), 0);

    for (std::size_t i = 0; i < config_.emission.bursts.size(); ++i) {
        const auto& burst = config_.emission.bursts[i];
        int& nextCycle = burstNextCycle_[i];

        while (nextCycle < burst.cycleCount) {
            const float dueTime = burst.time + burst.repeatInterval * nextCycle;
            if (dueTime < prevTime)
            {
                ++nextCycle;
                continue;
            }
            if (dueTime >= currTime)
                break;

            if (randomFloat(0.f, 1.f) <= burst.probability) {
                const int lo = std::min(burst.countMin, burst.countMax);
                const int hi = std::max(burst.countMin, burst.countMax);
                const int count = std::uniform_int_distribution<int>{ lo, hi }(rng_);
                emit(count);
            }
            ++nextCycle;
        }
    }
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
            const float prevTime = systemTime_;
            float       nextTime = systemTime_ + scaledDtf;

            if (config_.emission.enabled && config_.emission.emitRate > 0.f) {
                emitAccumNew_ += config_.emission.emitRate * scaledDtf;
                const int n = static_cast<int>(emitAccumNew_);
                emitAccumNew_ -= static_cast<float>(n);
                if (n > 0) emit(n);
            }

            if (config_.main.duration > 0.f) {
                if (nextTime >= config_.main.duration) {
                    emitScheduledBursts(prevTime, config_.main.duration);
                    if (config_.main.looping) {
                        nextTime -= config_.main.duration;
                        burstNextCycle_.assign(config_.emission.bursts.size(), 0);
                        emitScheduledBursts(0.f, nextTime);
                        systemTime_ = nextTime;
                    } else {
                        continuousNew_ = false;
                        systemTime_    = 0.f;
                    }
                } else {
                    emitScheduledBursts(prevTime, nextTime);
                    systemTime_ = nextTime;
                }
            } else {
                emitScheduledBursts(prevTime, nextTime);
                systemTime_ = nextTime;
            }
        }
    }

    int i = 0;
    while (i < activeCount_) {
        Particle& p = pool_[i];

        p.vel      *= std::max(0.f, 1.f - p.drag * scaledDtf);
        p.vel      += p.gravity * scaledDtf;
        p.motionVelocity = p.vel + calcVelocityOverLifetime(config_, p);
        p.pos      += p.motionVelocity * scaledDtf;
        p.lifetime -= scaledDtf;

        if (p.angularVelocity != 0.f)
            p.angularAngle += p.angularVelocity * scaledDtf;

        if (config_.rotationOverLifetime.enabled && config_.rotationOverLifetime.useCurves) {
            const float t = 1.f - p.lifetime / p.maxLifetime;
            p.angularAngle3D += mu::Vec3{
                config_.rotationOverLifetime.x.evaluate(t, p.rotationRandom3D.x()),
                config_.rotationOverLifetime.y.evaluate(t, p.rotationRandom3D.y()),
                config_.rotationOverLifetime.z.evaluate(t, p.rotationRandom3D.z())
            } * scaledDtf;
        } else {
            p.angularAngle3D += p.angularVelocity3D * scaledDtf;
        }

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

    ParticleRenderFrameState frameState{};

    for (int i = 0; i < activeCount_; ++i) {
        const Particle& p    = pool_[i];
        const float    t     = 1.f - p.lifetime / p.maxLifetime;
        const float    size  = std::lerp(p.sizeBegin, p.sizeEnd, t);
        const mu::Vec4 tint  = p.startColor * p.colorOverLifetime.evaluate(t);
        const bool     customDataEnabled = config_.customData.enabled;
        const mu::Vec2 custom1 = customDataEnabled
                               ? evaluateCustomDataStream(config_.customData.custom1, t, p.custom1Random)
                               : mu::Vec2{ 0.f, 0.f };
        const mu::Vec2 custom2 = customDataEnabled
                               ? evaluateCustomDataStream(config_.customData.custom2, t, p.custom2Random)
                               : mu::Vec2{ 0.f, 0.f };

        ParticleRenderContext ctx{
            .renderer = rend,
            .textureSheetAnimation = tsa,
            .particle = p,
            .frameState = frameState,
            .systemTime = systemTime_,
            .t = t,
            .size = size,
            .tint = tint,
            .custom1 = custom1,
            .custom2 = custom2,
            .customDataEnabled = customDataEnabled,
        };

        std::visit([&](const auto& mat) {
            submitParticleDraw(gfx, ctx, mat);
        }, rend.mat);
    }
}

// ── New module-based API implementation ─────────────────────────────────────

void ParticleSystem::init(const ps::ParticleSystemConfig& config, int maxParticles) {
    config_       = config;
    maxParticles_ = (config_.main.maxParticles > 0) ? config_.main.maxParticles : maxParticles;
    pool_.resize(maxParticles_);
    activeCount_     = 0;
    overwriteCursor_ = 0;
    emitAccumNew_    = 0.f;
    continuousNew_   = false;
    systemTime_      = 0.f;
    delayRemaining_  = 0.f;
    burstNextCycle_.assign(config_.emission.bursts.size(), 0);
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
    burstNextCycle_.assign(config_.emission.bursts.size(), 0);
}

void ParticleSystem::startContinuous(const ps::ParticleSystemConfig& config) {
    init(config);
    startContinuous();
}

// ── Shape sampling ───────────────────────────────────────────────────────────

mu::Vec3 ParticleSystem::sampleShapeOrigin() {
    const ps::ShapeModule& s = config_.shape;
    if (!s.enabled)
        return s.position;

    auto withRandomOffset = [&](mu::Vec3 p) {
        if (s.randomPositionAmount <= 0.f)
            return p;
        const mu::Vec3 randomDir = sampleConeDirection(mu::NVec3(mu::Vec3{ 0.f, 1.f, 0.f }),
                                                       3.14159265f, rng_);
        return p + randomDir * randomFloat(0.f, s.randomPositionAmount);
    };

    switch (s.type) {
    case ps::ShapeModule::Type::Point:
        return withRandomOffset(s.position);

    case ps::ShapeModule::Type::Edge: {
        const float lenSq = mu::dot(s.edgeDir, s.edgeDir);
        if (lenSq < 1e-6f) return s.position;
        const mu::Vec3 dir = rotateShapeDirection(s.edgeDir, s);
        return withRandomOffset(s.position + dir * randomFloat(-s.edgeLength * 0.5f, s.edgeLength * 0.5f));
    }

    case ps::ShapeModule::Type::Circle: {
        const float angle = randomFloat(0.f, s.arc);
        const float inner = std::clamp(1.f - s.radiusThickness, 0.f, 1.f);
        const float r     = s.coneRadius * std::sqrt(randomFloat(inner * inner, 1.f));
        const mu::Vec3 localOffset = { r * std::cos(angle), r * std::sin(angle), 0.f };
        return withRandomOffset(s.position + rotateShapeVector(localOffset, s));
    }

    case ps::ShapeModule::Type::Cone:
        if (s.coneRadius > 0.f) {
            // random point on base disc; disc is perpendicular to the up axis by default
            const float angle = randomFloat(0.f, 2.f * 3.14159265f);
            const float r     = s.coneRadius * std::sqrt(randomFloat(0.f, 1.f));
            const mu::Vec3 localOffset = { r * std::cos(angle), 0.f, r * std::sin(angle) };
            return withRandomOffset(s.position + rotateShapeVector(localOffset, s));
        }
        return withRandomOffset(s.position);  // apex emit

    case ps::ShapeModule::Type::Sphere: {
        const float theta = 2.f * 3.14159265f * randomFloat(0.f, 1.f);
        const float phi   = std::acos(1.f - 2.f * randomFloat(0.f, 1.f));
        const float sp    = std::sin(phi);
        return withRandomOffset(s.position + mu::Vec3{
            s.sphereRadius * sp * std::cos(theta),
            s.sphereRadius * std::cos(phi),
            s.sphereRadius * sp * std::sin(theta) });
    }

    case ps::ShapeModule::Type::Box:
        return withRandomOffset(s.position + rotateShapeVector(mu::Vec3{
            randomFloat(-s.boxSize.x() * 0.5f,  s.boxSize.x() * 0.5f),
            randomFloat(-s.boxSize.y() * 0.5f,  s.boxSize.y() * 0.5f),
            randomFloat(-s.boxSize.z() * 0.5f,  s.boxSize.z() * 0.5f) }, s));
    }
    return s.position;
}

mu::Vec3 ParticleSystem::sampleShapeDirection(const mu::Vec3& origin) {
    const ps::ShapeModule& s = config_.shape;
    if (!s.enabled)
        return s.direction;

    switch (s.type) {
    case ps::ShapeModule::Type::Point:
        // Point: emit along the configured direction axis with no spread
        return rotateShapeDirection(s.direction, s);

    case ps::ShapeModule::Type::Edge:
        // Emit upward (perpendicular to edge) along the configured direction
        return rotateShapeDirection(s.direction, s);

    case ps::ShapeModule::Type::Cone:
        return sampleConeDirection(mu::NVec3(rotateShapeDirection(s.direction, s)), s.coneAngle, rng_);

    case ps::ShapeModule::Type::Circle:
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

    p.pos             = origin;
    p.vel             = dir * speed;
    p.emitterPosition = config_.shape.position;
    p.emitterRotation = buildShapeRotation(config_.shape);
    p.motionVelocity  = p.vel + calcVelocityOverLifetime(config_, p);
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
                        ? (config_.rotationOverLifetime.separateAxes
                            ? randomFloat(config_.rotationOverLifetime.angularVelocityMin3D.z(),
                                          config_.rotationOverLifetime.angularVelocityMax3D.z())
                            : randomFloat(config_.rotationOverLifetime.angularVelocityMin,
                                          config_.rotationOverLifetime.angularVelocityMax))
                        : 0.f;
    if (config_.rotationOverLifetime.enabled && config_.rotationOverLifetime.separateAxes) {
        p.angularVelocity3D = {
            randomFloat(config_.rotationOverLifetime.angularVelocityMin3D.x(),
                        config_.rotationOverLifetime.angularVelocityMax3D.x()),
            randomFloat(config_.rotationOverLifetime.angularVelocityMin3D.y(),
                        config_.rotationOverLifetime.angularVelocityMax3D.y()),
            p.angularVelocity
        };
    } else {
        p.angularVelocity3D = { 0.f, 0.f, p.angularVelocity };
    }
    p.angularAngle    = 0.f;
    p.angularAngle3D  = { 0.f, 0.f, 0.f };
    p.rotationRandom3D = { randomFloat(0.f, 1.f), randomFloat(0.f, 1.f), randomFloat(0.f, 1.f) };
    p.baseRotation    = config_.main.startRotation3D;
    if (config_.main.startRotation3DEnabled) {
        const mu::Vec3 startRotation = {
            randomFloat(config_.main.startRotation3DMin.x(), config_.main.startRotation3DMax.x()),
            randomFloat(config_.main.startRotation3DMin.y(), config_.main.startRotation3DMax.y()),
            randomFloat(config_.main.startRotation3DMin.z(), config_.main.startRotation3DMax.z())
        };
        p.baseRotation = buildEulerRotation(startRotation) * p.baseRotation;
    }
    p.custom1Random   = { randomFloat(0.f, 1.f), randomFloat(0.f, 1.f) };
    p.custom2Random   = { randomFloat(0.f, 1.f), randomFloat(0.f, 1.f) };
}
