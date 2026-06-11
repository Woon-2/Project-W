#include "pch.hpp"
#include "particleSystem.hpp"
#include "particleRenderSubmit.hpp"


// Samples a random direction within a cone of half-angle `spread` around `axis`.
// `arc` (radians, default 2pi) restricts the azimuth range around the cone axis.
// `tangentHint` (default zero = unset) sets the world-space phi=0 reference direction;
// it is orthogonalized against `axis`. When unset, falls back to a stable arbitrary tangent.
// bitangent = cross(tangent, axis) so that (cos,sin) matches sampleShapeOrigin Cone's XZ convention.
static mu::Vec3 sampleConeDirection(mu::NVec3 axis, float spread, std::mt19937& rng,
                                     float arc = 2.f * 3.14159265f,
                                     mu::Vec3 tangentHint = { 0.f, 0.f, 0.f }) {
    std::uniform_real_distribution<float> u01(0.f, 1.f);
    const float cosSpread = std::cos(spread);

    // uniform sample on spherical cap: cos(theta) in [cosSpread, 1]
    const float cosTheta = cosSpread + (1.f - cosSpread) * u01(rng);
    const float sinTheta = std::sqrt(1.f - cosTheta * cosTheta);
    const float phi      = arc * u01(rng);

    const mu::Vec3 axisV = mu::Vec3(axis);

    // Build tangent: use tangentHint projected onto the plane perpendicular to axis,
    // falling back to an arbitrary stable tangent when hint is unset or parallel to axis.
    mu::Vec3 tangent;
    bool hintUsed = false;
    if (mu::dot(tangentHint, tangentHint) > 1e-6f) {
        mu::Vec3 t = tangentHint - axisV * mu::dot(tangentHint, axisV);
        const float tLenSq = mu::dot(t, t);
        if (tLenSq > 1e-6f) {
            tangent  = t * (1.f / std::sqrt(tLenSq));
            hintUsed = true;
        }
    }
    if (!hintUsed) {
        const mu::Vec3 up = (std::abs(axisV.x()) < 0.9f)
                          ? mu::Vec3{ 1.f, 0.f, 0.f }
                          : mu::Vec3{ 0.f, 1.f, 0.f };
        tangent = mu::Vec3(mu::cross(axis, mu::NVec3(up)));
    }
    const mu::Vec3 bitangent = mu::Vec3(mu::cross(mu::NVec3(tangent), axis));

    return sinTheta * std::cos(phi) * tangent
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

// Rotation that maps world up (+Y) onto the given (assumed unit) surface normal.
// Used by ShapeModule::GroundConform::SnapAndAlign to tilt mesh particles to the
// terrain slope. Returns identity when the normal is already (near) vertical.
static mu::Mat4x4 alignYToNormalMat(mu::Vec3 n) {
    const mu::Vec3 up{ 0.f, 1.f, 0.f };
    const mu::Vec3 axis = mu::cross(up, n);
    const float    s2   = mu::dot(axis, axis);
    if (s2 < 1e-8f)
        return mu::Mat4x4{};  // parallel (or anti-parallel) to up: no tilt
    const float c     = std::clamp(mu::dot(up, n), -1.f, 1.f);
    const float angle = std::acos(c);
    return mu::rotateH(mu::Radian{ angle }, mu::Vec3(mu::normalize(axis)));
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

static float maxComponent(mu::Vec3 v) {
    return std::max(v.x(), std::max(v.y(), v.z()));
}

static mu::Vec3 lerpVec3(mu::Vec3 a, mu::Vec3 b, float t) {
    return {
        std::lerp(a.x(), b.x(), t),
        std::lerp(a.y(), b.y(), t),
        std::lerp(a.z(), b.z(), t)
    };
}

static mu::Vec3 calcVelocityOverLifetime(
    const ps::ParticleSystemConfig& cfg, const Particle& p
) {
    const auto& vol = cfg.velocityOverLifetime;
    if (!vol.enabled)
        return { 0.f, 0.f, 0.f };

    const float t = 1.f - p.lifetime / p.maxLifetime;
    const mu::Vec3 linear = vol.useCurves
        ? mu::Vec3{
            vol.linearX.evaluate(t, p.velocityRandom3D.x()),
            vol.linearY.evaluate(t, p.velocityRandom3D.y()),
            vol.linearZ.evaluate(t, p.velocityRandom3D.z())
        }
        : vol.linear;
    const mu::Vec3 orbital = vol.useCurves
        ? mu::Vec3{
            vol.orbitalX.evaluate(t, p.orbitalRandom3D.x()),
            vol.orbitalY.evaluate(t, p.orbitalRandom3D.y()),
            vol.orbitalZ.evaluate(t, p.orbitalRandom3D.z())
        }
        : vol.orbital;
    const float radial = vol.useCurves
        ? vol.radialCurve.evaluate(t, p.velocityRandomExtra.x())
        : vol.radial;
    const float speedModifier = vol.useCurves
        ? vol.speedModifierCurve.evaluate(t, p.velocityRandomExtra.y())
        : vol.speedModifier;

    const mu::Vec3 radialOffset = p.pos - p.emitterPosition;
    const float radius = std::sqrt(mu::dot(radialOffset, radialOffset));
    const mu::Vec3 radialDir = (radius > 1e-6f)
                             ? radialOffset * (1.f / radius)
                             : rotateDirection(cfg.shape.direction, p.emitterRotation);

    const mu::Vec3 localLinear = vol.inWorldSpace
                               ? linear
                               : rotateVector(linear, p.emitterTransformRotation);

    mu::Vec3 velocity = localLinear + radialDir * radial;

    auto addOrbitalVelocity = [&](mu::Vec3 localAxis, float angularSpeed) {
        if (std::abs(angularSpeed) <= 1e-6f || radius <= 1e-6f)
            return;

        const mu::Vec3 axis = vol.inWorldSpace
                            ? localAxis
                            : rotateDirection(localAxis, p.emitterTransformRotation);
        const mu::Vec3 tangentRaw = mu::cross(axis, radialDir);
        const float tangentLenSq = mu::dot(tangentRaw, tangentRaw);
        if (tangentLenSq > 1e-6f) {
            const mu::Vec3 tangent = tangentRaw * (1.f / std::sqrt(tangentLenSq));
            velocity += tangent * (angularSpeed * radius);
        }
    };

    addOrbitalVelocity({ 1.f, 0.f, 0.f }, orbital.x());
    addOrbitalVelocity({ 0.f, 1.f, 0.f }, orbital.y());
    addOrbitalVelocity({ 0.f, 0.f, 1.f }, orbital.z());

    return velocity * speedModifier;
}

float ParticleSystem::randomFloat(float lo, float hi) {
    if (hi < lo)
        std::swap(lo, hi);
    return std::uniform_real_distribution<float>{lo, hi}(rng_);
}

float ParticleSystem::sampleArcAngle() {
    const auto& s = config_.shape;
    if (s.arcMode != 3 || shapeEmitCount_ <= 1)
        return randomFloat(0.f, s.arc);

    const bool fullCircle = std::abs(s.arc - 2.f * 3.14159265f) < 1e-4f;
    const float denom = fullCircle
                      ? static_cast<float>(shapeEmitCount_)
                      : static_cast<float>(shapeEmitCount_ - 1);
    return s.arc * static_cast<float>(shapeEmitIndex_) / std::max(1.f, denom);
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

// Deterministic-mode burst scheduler. Window logic mirrors the legacy
// emitScheduledBursts; probability/count rolls and per-particle draws use
// counter-keyed RNG so the server reproduces them (pg::evaluateParticles).
void ParticleSystem::emitScheduledBurstsDet(float prevTime, float currTime) {
    const pg::GameplayConfig& g = gameplayCfg_;
    if (!g.emissionEnabled || g.bursts.empty())
        return;

    if (burstNextCycle_.size() != g.bursts.size())
        burstNextCycle_.assign(g.bursts.size(), 0);

    for (std::size_t i = 0; i < g.bursts.size(); ++i) {
        const pg::Burst& burst = g.bursts[i];
        int& nextCycle = burstNextCycle_[i];

        while (nextCycle < burst.cycleCount) {
            const float dueTime = burst.time + burst.repeatInterval * nextCycle;
            if (dueTime < prevTime) {
                ++nextCycle;
                continue;
            }
            if (dueTime >= currTime)
                break;

            const std::uint32_t key = pg::burstKey(detLoopIndex_,
                                                   static_cast<int>(i), nextCycle);
            pg::DetRng meta{ detSeed_, pg::kStreamBurstMeta, key, 0 };
            if (meta.next01() <= burst.probability) {
                const int lo = std::min(burst.countMin, burst.countMax);
                const int hi = std::max(burst.countMin, burst.countMax);
                const int count = (lo >= hi)
                    ? lo
                    : lo + static_cast<int>(std::floor(meta.next01()
                            * static_cast<float>(hi - lo + 1)));

                const int savedIndex = shapeEmitIndex_;
                const int savedCount = shapeEmitCount_;
                shapeEmitCount_ = std::max(1, count);
                for (int p = 0; p < count; ++p) {
                    shapeEmitIndex_  = p;
                    detSpawnStream_  = pg::kStreamBurstSpawn;
                    detSpawnId_      = pg::burstParticleId(key, p);
                    detSpawnPending_ = true;
                    spawnParticle();
                }
                detSpawnPending_ = false;
                shapeEmitIndex_ = savedIndex;
                shapeEmitCount_ = savedCount;
            }
            ++nextCycle;
        }
    }
}

// Deterministic-mode rate emission: the k-th particle exists once the
// cumulative target floor(rate * t) passes k, independent of frame splits.
void ParticleSystem::emitRateDet() {
    const pg::GameplayConfig& g = gameplayCfg_;
    if (!g.emissionEnabled || g.emitRate <= 0.f)
        return;

    const bool  bounded    = (!g.looping && g.duration > 0.f);
    const float emitWindow = bounded ? std::min(detSysTime_, g.duration) : detSysTime_;
    const int   total      = static_cast<int>(std::floor(g.emitRate * emitWindow));

    const int savedIndex = shapeEmitIndex_;
    const int savedCount = shapeEmitCount_;
    shapeEmitCount_ = 1;
    shapeEmitIndex_ = 0;
    while (static_cast<int>(detRateIndex_) < total) {
        detSpawnStream_  = pg::kStreamRate;
        detSpawnId_      = detRateIndex_++;
        detSpawnPending_ = true;
        spawnParticle();
    }
    detSpawnPending_ = false;
    shapeEmitIndex_ = savedIndex;
    shapeEmitCount_ = savedCount;
}

void ParticleSystem::stopContinuous() {
    continuousNew_ = false;
}

void ParticleSystem::update(Seconds dt) {
    const float scaledDtf = dt.count() * config_.main.simulationSpeed;

    // Start Delay -> Emission -> Duration / Looping
    if (continuousNew_ && deterministic()) {
        // Deterministic mode: schedule and draws are driven by the gameplay
        // config (shared with the server) and counter-keyed RNG, so spawn
        // counts and parameters are frame-partition independent.
        const pg::GameplayConfig& g = gameplayCfg_;
        if (delayRemaining_ > 0.f) {
            delayRemaining_ -= scaledDtf;
        } else {
            const float prevTime = systemTime_;
            float       nextTime = systemTime_ + scaledDtf;
            detSysTime_ += scaledDtf;

            emitRateDet();

            if (g.duration > 0.f) {
                if (nextTime >= g.duration) {
                    emitScheduledBurstsDet(prevTime, g.duration);
                    if (g.looping) {
                        nextTime -= g.duration;
                        burstNextCycle_.assign(g.bursts.size(), 0);
                        ++detLoopIndex_;
                        emitScheduledBurstsDet(0.f, nextTime);
                        systemTime_ = nextTime;
                    } else {
                        continuousNew_ = false;
                        systemTime_    = 0.f;
                    }
                } else {
                    emitScheduledBurstsDet(prevTime, nextTime);
                    systemTime_ = nextTime;
                }
            } else {
                emitScheduledBurstsDet(prevTime, nextTime);
                systemTime_ = nextTime;
            }
        }
    } else if (continuousNew_) {
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

        // Terrain collision. Only relevant for descending/static particles, so
        // gate the (cheap but non-free) ground query on a downward velocity for
        // the modes that imply falling. GroundKill funnels into the existing
        // lifetime<=0 death path below, which fires Death sub-emitters at p.pos.
        if (config_.collision.enabled && ground_ && *ground_
            && config_.collision.mode != ps::ParticleCollisionModule::Mode::None
            && (p.motionVelocity.y() < 0.f
                || config_.collision.mode == ps::ParticleCollisionModule::Mode::GroundStop)) {
            const float surfY = ground_->height(p.pos.x(), p.pos.z()) + config_.collision.radiusOffset;
            if (p.pos.y() <= surfY) {
                switch (config_.collision.mode) {
                case ps::ParticleCollisionModule::Mode::GroundStop:
                    p.pos.setComponent(1, surfY);
                    p.vel = {}; p.gravity = {}; p.motionVelocity = {};
                    break;
                case ps::ParticleCollisionModule::Mode::GroundBounce:
                    p.pos.setComponent(1, surfY);
                    p.vel.setComponent(1, -p.vel.y() * config_.collision.bounce);
                    break;
                case ps::ParticleCollisionModule::Mode::GroundKill:
                    p.lifetime = 0.f;
                    break;
                default: break;
                }
            }
        }

        // Trail capture + ageing. Stored coordinate follows MainModule::simulationSpace
        // (World -> world-space; Local -> emitter-local). For Local space we apply
        // emitter inverse at capture so draw-time only needs the current emitter transform.
        if (p.trailEnabled) {
            constexpr int kCap = Particle::kMaxTrailSegments;
            mu::Vec3 capturePos = p.pos;
            // NOTE: Local-space simulation currently stores world positions because
            // the existing simulation pipeline keeps p.pos in world coords even when
            // simulationSpace == Local. When emitter-relative storage is needed we'll
            // transform here against emitterPosition/emitterRotation. (Deferred.)

            // Distance gate: emit a new vertex only when we've moved far enough from
            // the most recent recorded point. minVertexDistance <= 0 disables the gate.
            bool acceptNew = true;
            if (p.trailCount > 0 && config_.trail.minVertexDistance > 0.f) {
                const int lastIdx = (p.trailHead - 1 + kCap) % kCap;
                const mu::Vec3 d  = capturePos - p.trail[lastIdx].pos;
                const float    d2 = mu::dot(d, d);
                const float    md = config_.trail.minVertexDistance;
                acceptNew = (d2 >= md * md);
            }
            if (acceptNew) {
                p.trail[p.trailHead] = TrailPoint{ capturePos, systemTime_ };
                p.trailHead = static_cast<std::uint8_t>((p.trailHead + 1) % kCap);
                if (p.trailCount < kCap) ++p.trailCount;
                // If buffer is full, oldest entry is overwritten implicitly by the
                // head advance; trailCount stays at kCap.
            }

            // Age out tail points older than trailLifetime.
            while (p.trailCount > 0) {
                const int tailIdx = (p.trailHead - p.trailCount + kCap) % kCap;
                if (systemTime_ - p.trail[tailIdx].spawnTime > p.trailLifetime) {
                    --p.trailCount;
                } else {
                    break;
                }
            }
        }

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
            if (config_.subEmitters.enabled) {
                for (int j = 0; j < (int)config_.subEmitters.subEmitters.size(); ++j) {
                    const auto& sub = config_.subEmitters.subEmitters[j];
                    if (sub.event == ps::SubEmittersModule::Event::Death
                        && randomFloat(0.f, 1.f) <= sub.emitProbability) {
                        pendingSubEmitterEvents_.push_back({ j, p.pos, p.vel, p.startColor, p.sizeStart });
                    }
                }
            }
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
        const mu::Vec3 size3D = (config_.sizeOverLifetime.enabled && config_.sizeOverLifetime.useCurve)
                              ? p.sizeStart3D * config_.sizeOverLifetime.size.evaluate(t, p.sizeRandom)
                              : lerpVec3(p.sizeBegin3D, p.sizeEnd3D, t);
        const float    size  = maxComponent(size3D);
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
            .size3D = size3D,
            .tint = tint,
            .custom1 = custom1,
            .custom2 = custom2,
            .customDataEnabled = customDataEnabled,
        };

        std::visit([&](const auto& mat) {
            submitParticleDraw(gfx, ctx, mat);
        }, rend.mat);

        // Trails are an additive overlay independent of the renderer mode —
        // submit alongside the primary draw. submitParticleTrail() is a no-op
        // when the TrailModule is disabled or this particle has no trail.
        submitParticleTrail(gfx, ctx, config_.trail);
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
    const int savedIndex = shapeEmitIndex_;
    const int savedCount = shapeEmitCount_;
    shapeEmitCount_ = std::max(1, count);
    for (int i = 0; i < count; ++i) {
        shapeEmitIndex_ = i;
        if (deterministic() && !inheritEmit_) {
            // Manual emission (ParticleEffect::play PlayMode::Emit) uses the
            // play-emit stream; the server mirrors id 0 for the play() call.
            detSpawnStream_  = pg::kStreamPlayEmit;
            detSpawnId_      = detEmitIndex_++;
            detSpawnPending_ = true;
        }
        spawnParticle();
    }
    detSpawnPending_ = false;
    shapeEmitIndex_ = savedIndex;
    shapeEmitCount_ = savedCount;
}

void ParticleSystem::startContinuous() {
    continuousNew_  = true;
    emitAccumNew_   = 0.f;
    systemTime_     = 0.f;
    delayRemaining_ = deterministic() ? gameplayCfg_.startDelay
                                      : config_.main.startDelay;
    burstNextCycle_.assign(deterministic() ? gameplayCfg_.bursts.size()
                                           : config_.emission.bursts.size(), 0);
    detRateIndex_ = 0;
    detEmitIndex_ = 0;
    detLoopIndex_ = 0;
    detSysTime_   = 0.f;
}

void ParticleSystem::startContinuous(const ps::ParticleSystemConfig& config) {
    init(config);
    startContinuous();
}

// ── Shape sampling ───────────────────────────────────────────────────────────

// Circle: origin and dir share the same arc angle so direction is always the
// correct radial unit vector regardless of how small the radius is.
ParticleSystem::ShapeSample ParticleSystem::sampleCircle() {
    const ps::ShapeModule& s = config_.shape;
    const float angle = sampleArcAngle();
    const float inner = std::clamp(1.f - s.radiusThickness, 0.f, 1.f);
    const float r     = s.coneRadius * std::sqrt(randomFloat(inner * inner, 1.f));
    const mu::Vec3 localDir = { std::cos(angle), std::sin(angle), 0.f };

    const auto withRandomOffset = [&](mu::Vec3 p) {
        if (s.randomPositionAmount <= 0.f) return p;
        const mu::Vec3 randomDir = sampleConeDirection(mu::NVec3(mu::Vec3{ 0.f, 1.f, 0.f }),
                                                       3.14159265f, rng_);
        return p + randomDir * randomFloat(0.f, s.randomPositionAmount);
    };

    return {
        withRandomOffset(s.position + rotateShapeVector(localDir * r, s)),
        rotateShapeVector(localDir, s)
    };
}

// Unity Cone uses local +Z as the cone axis. Angle is the side angle from that
// axis, and Arc selects the azimuth around it. Keep origin and direction on the
// same azimuth so partial arcs emit as coherent fan wedges.
ParticleSystem::ShapeSample ParticleSystem::sampleCone() {
    const ps::ShapeModule& s = config_.shape;
    const float angle = sampleArcAngle();
    const float inner = std::clamp(1.f - s.radiusThickness, 0.f, 1.f);
    const float r     = s.coneRadius * std::sqrt(randomFloat(inner * inner, 1.f));

    const float sideSin = std::sin(s.coneAngle);
    const float sideCos = std::cos(s.coneAngle);
    const mu::Vec3 localRadial = { std::cos(angle), std::sin(angle), 0.f };
    const mu::Vec3 localOffset = localRadial * r;
    const mu::Vec3 localDir = {
        localRadial.x() * sideSin,
        localRadial.y() * sideSin,
        sideCos
    };

    const auto withRandomOffset = [&](mu::Vec3 p) {
        if (s.randomPositionAmount <= 0.f) return p;
        const mu::Vec3 randomDir = sampleConeDirection(mu::NVec3(mu::Vec3{ 0.f, 1.f, 0.f }),
                                                       3.14159265f, rng_);
        return p + randomDir * randomFloat(0.f, s.randomPositionAmount);
    };

    return {
        withRandomOffset(s.position + rotateShapeVector(localOffset, s)),
        rotateShapeDirection(localDir, s)
    };
}

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
        const float angle = sampleArcAngle();
        const float inner = std::clamp(1.f - s.radiusThickness, 0.f, 1.f);
        const float r     = s.coneRadius * std::sqrt(randomFloat(inner * inner, 1.f));
        const mu::Vec3 localOffset = { r * std::cos(angle), r * std::sin(angle), 0.f };
        return withRandomOffset(s.position + rotateShapeVector(localOffset, s));
    }

    case ps::ShapeModule::Type::Cone:
        if (s.coneRadius > 0.f) {
            const float angle = sampleArcAngle();
            const float inner = std::clamp(1.f - s.radiusThickness, 0.f, 1.f);
            const float r     = s.coneRadius * std::sqrt(randomFloat(inner * inner, 1.f));
            const mu::Vec3 localOffset = { r * std::cos(angle), r * std::sin(angle), 0.f };
            return withRandomOffset(s.position + rotateShapeVector(localOffset, s));
        }
        return withRandomOffset(s.position);  // apex emit

    case ps::ShapeModule::Type::Sphere: {
        const float theta = 2.f * 3.14159265f * randomFloat(0.f, 1.f);
        const float phi   = std::acos(1.f - 2.f * randomFloat(0.f, 1.f));
        const float sp    = std::sin(phi);
        const float inner = std::clamp(1.f - s.radiusThickness, 0.f, 1.f);
        const float r     = s.sphereRadius * std::cbrt(randomFloat(inner * inner * inner, 1.f));
        return withRandomOffset(s.position + mu::Vec3{
            r * sp * std::cos(theta),
            r * std::cos(phi),
            r * sp * std::sin(theta) });
    }

    case ps::ShapeModule::Type::Hemisphere: {
        const float theta = 2.f * 3.14159265f * randomFloat(0.f, 1.f);
        const float y     = randomFloat(0.f, 1.f);
        const float sp    = std::sqrt(std::max(0.f, 1.f - y * y));
        const float inner = std::clamp(1.f - s.radiusThickness, 0.f, 1.f);
        const float r     = s.sphereRadius * std::cbrt(randomFloat(inner * inner * inner, 1.f));
        const mu::Vec3 localOffset = {
            r * sp * std::cos(theta),
            r * y,
            r * sp * std::sin(theta)
        };
        return withRandomOffset(s.position + rotateShapeVector(localOffset, s));
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
    {
        const float angle = sampleArcAngle();
        const float sideSin = std::sin(s.coneAngle);
        const float sideCos = std::cos(s.coneAngle);
        return rotateShapeDirection({
            std::cos(angle) * sideSin,
            std::sin(angle) * sideSin,
            sideCos
        }, s);
    }

    case ps::ShapeModule::Type::Circle:
    case ps::ShapeModule::Type::Sphere:
    case ps::ShapeModule::Type::Hemisphere:
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

    // Deterministic gameplay mode: spawn parameters that affect hitboxes come
    // from the shared sampler (counter-keyed RNG; see particleGameplay.hpp).
    // Visual-only draws below keep consuming the legacy rng_.
    const bool det = deterministic() && detSpawnPending_ && !inheritEmit_;
    pg::SpawnParams sp;
    if (det) {
        pg::DetRng detRng{ detSeed_, detSpawnStream_, detSpawnId_, 0 };
        sp = pg::sampleSpawn(gameplayCfg_, config_.shape.position,
                             config_.shape.orientation, detRng,
                             shapeEmitIndex_, shapeEmitCount_);
    }

    mu::Vec3 origin, dir;
    if (det) {
        origin = sp.origin;
        dir    = sp.dir;
    } else if (config_.shape.enabled && config_.shape.type == ps::ShapeModule::Type::Circle) {
        const auto s = sampleCircle();
        origin = s.origin;
        dir    = s.dir;
    } else if (config_.shape.enabled && config_.shape.type == ps::ShapeModule::Type::Cone) {
        const auto s = sampleCone();
        origin = s.origin;
        dir    = s.dir;
    } else {
        origin = sampleShapeOrigin();
        dir    = sampleShapeDirection(origin);
    }

    // Ground conform: snap the spawn Y to the terrain surface at the sampled XZ.
    // Skipped when no sampler is bound (e.g. terrain not loaded) so particles are
    // not yanked to y = 0.
    if (config_.shape.groundConform != ps::ShapeModule::GroundConform::None
        && ground_ && *ground_) {
        origin.setComponent(1, ground_->height(origin.x(), origin.z()) + config_.shape.groundOffset);
    }

    const float    speed  = det ? sp.speed
                                : randomFloat(config_.main.speedMin, config_.main.speedMax);

    p.pos             = origin;
    p.vel             = dir * speed;
    p.emitterPosition = config_.shape.position;
    p.emitterRotation = buildShapeRotation(config_.shape);
    p.emitterTransformRotation = config_.shape.orientation;
    p.lifetime    = det ? sp.lifetime
                        : randomFloat(config_.main.lifetimeMin, config_.main.lifetimeMax);
    p.maxLifetime = p.lifetime;
    p.velocityRandom3D = { randomFloat(0.f, 1.f), randomFloat(0.f, 1.f), randomFloat(0.f, 1.f) };
    p.orbitalRandom3D = { randomFloat(0.f, 1.f), randomFloat(0.f, 1.f), randomFloat(0.f, 1.f) };
    p.velocityRandomExtra = { randomFloat(0.f, 1.f), randomFloat(0.f, 1.f) };
    p.motionVelocity  = p.vel + calcVelocityOverLifetime(config_, p);
    p.startColor  = config_.main.startColor;
    p.drag        = config_.velocityOverLifetime.enabled ? config_.velocityOverLifetime.drag : 0.f;
    p.gravity     = config_.main.gravity *
                    (det ? sp.gravityModifier
                         : randomFloat(config_.main.gravityModifierMin,
                                       config_.main.gravityModifierMax));
    p.rotation    = det ? sp.rotationZ
                        : randomFloat(config_.main.startRotationMin, config_.main.startRotationMax);

    if (config_.colorOverLifetime.enabled)
        p.colorOverLifetime = config_.colorOverLifetime.gradient;
    else
        p.colorOverLifetime = ColorGradient::constant({ 1.f, 1.f, 1.f, 1.f });

    const float scalarSizeMult = det ? sp.sizeScalar
                                     : randomFloat(config_.main.startSizeMin, config_.main.startSizeMax);
    const mu::Vec3 sizeMult3D = det
                              ? sp.size3D
                              : config_.main.startSize3DEnabled
                              ? mu::Vec3{
                                  randomFloat(config_.main.startSize3DMin.x(), config_.main.startSize3DMax.x()),
                                  randomFloat(config_.main.startSize3DMin.y(), config_.main.startSize3DMax.y()),
                                  randomFloat(config_.main.startSize3DMin.z(), config_.main.startSize3DMax.z())
                                }
                              : mu::Vec3{ scalarSizeMult, scalarSizeMult, scalarSizeMult };
    const float sizeMult = maxComponent(sizeMult3D);
    p.sizeStart = sizeMult;
    p.sizeStart3D = sizeMult3D;
    p.sizeRandom = randomFloat(0.f, 1.f);
    if (config_.sizeOverLifetime.enabled) {
        p.sizeBegin = config_.sizeOverLifetime.sizeBegin * sizeMult;
        p.sizeEnd   = config_.sizeOverLifetime.sizeEnd   * sizeMult;
        p.sizeBegin3D = sizeMult3D * config_.sizeOverLifetime.sizeBegin;
        p.sizeEnd3D   = sizeMult3D * config_.sizeOverLifetime.sizeEnd;
    } else {
        p.sizeBegin = p.sizeEnd = sizeMult;
        p.sizeBegin3D = p.sizeEnd3D = sizeMult3D;
    }

    if (det) {
        p.angularVelocity   = sp.angularVelocityZ;
        p.angularVelocity3D = sp.angularVelocity3D;
    } else {
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
    }
    p.angularAngle    = 0.f;
    p.angularAngle3D  = { 0.f, 0.f, 0.f };
    p.rotationRandom3D = { randomFloat(0.f, 1.f), randomFloat(0.f, 1.f), randomFloat(0.f, 1.f) };
    p.baseRotation    = config_.main.startRotation3D;
    p.billboardRotation3D = mu::Mat4x4{};
    p.transformScale  = config_.main.transformScale;
    if (det ? sp.rotation3DEnabled : config_.main.startRotation3DEnabled) {
        const mu::Vec3 startRotation = det
            ? sp.rotation3D
            : mu::Vec3{
                randomFloat(config_.main.startRotation3DMin.x(), config_.main.startRotation3DMax.x()),
                randomFloat(config_.main.startRotation3DMin.y(), config_.main.startRotation3DMax.y()),
                randomFloat(config_.main.startRotation3DMin.z(), config_.main.startRotation3DMax.z())
            };
        const mu::Mat4x4 startRotationMatrix = buildEulerRotation(startRotation);
        p.baseRotation = startRotationMatrix * p.baseRotation;
        p.billboardRotation3D = startRotationMatrix;
    }
    // Ground conform (SnapAndAlign): tilt the base orientation to the terrain
    // slope so mesh particles (erupting pillars, decals) follow the surface.
    if (config_.shape.groundConform == ps::ShapeModule::GroundConform::SnapAndAlign
        && ground_ && *ground_) {
        p.baseRotation = p.baseRotation * alignYToNormalMat(ground_->normal(p.pos.x(), p.pos.z()));
    }
    p.custom1Random   = { randomFloat(0.f, 1.f), randomFloat(0.f, 1.f) };
    p.custom2Random   = { randomFloat(0.f, 1.f), randomFloat(0.f, 1.f) };

    if (inheritEmit_) {
        p.vel        += inheritedVel_;
        p.startColor  = p.startColor * inheritedColor_;
        p.sizeBegin  *= inheritedSize_;
        p.sizeEnd    *= inheritedSize_;
        p.sizeStart  *= inheritedSize_;
        p.sizeBegin3D *= inheritedSize_;
        p.sizeEnd3D   *= inheritedSize_;
        p.sizeStart3D *= inheritedSize_;
    }

    // Trail initialization
    p.trailHead    = 0;
    p.trailCount   = 0;
    p.trailEnabled = false;
    p.trailLifetime = 0.f;
    if (config_.trail.enabled) {
        const float roll = randomFloat(0.f, 1.f);
        if (roll <= config_.trail.ratio) {
            p.trailEnabled = true;
            p.trailLifetime = randomFloat(config_.trail.lifetimeMin, config_.trail.lifetimeMax);
            // Seed the ring buffer with the spawn position so the very first segment
            // can be drawn as soon as the second capture lands.
            p.trail[0] = TrailPoint{ p.pos, systemTime_ };
            p.trailHead  = 1;
            p.trailCount = 1;
        }
    }

    if (config_.subEmitters.enabled && !inheritEmit_) {
        for (int j = 0; j < (int)config_.subEmitters.subEmitters.size(); ++j) {
            const auto& sub = config_.subEmitters.subEmitters[j];
            if (sub.event == ps::SubEmittersModule::Event::Birth
                && randomFloat(0.f, 1.f) <= sub.emitProbability) {
                pendingSubEmitterEvents_.push_back({ j, p.pos, p.vel, p.startColor, p.sizeStart });
            }
        }
    }
}

void ParticleSystem::emitAt(int count, mu::Vec3 worldPos,
                             mu::Vec3 inheritVel, mu::Vec4 inheritColor, float inheritSize) {
    const mu::Vec3 savedPos = config_.shape.position;
    config_.shape.position  = worldPos;

    inheritEmit_    = true;
    inheritedVel_   = inheritVel;
    inheritedColor_ = inheritColor;
    inheritedSize_  = inheritSize;

    const int savedIndex = shapeEmitIndex_;
    const int savedCount = shapeEmitCount_;
    shapeEmitCount_ = std::max(1, count);
    for (int i = 0; i < count; ++i) {
        shapeEmitIndex_ = i;
        spawnParticle();
    }
    shapeEmitIndex_ = savedIndex;
    shapeEmitCount_ = savedCount;

    inheritEmit_           = false;
    config_.shape.position = savedPos;
}
