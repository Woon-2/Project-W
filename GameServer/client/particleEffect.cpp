#include "pch.hpp"
#include "particleEffect.hpp"
#include <algorithm>

namespace {

mu::Vec3 normalizedOr(mu::Vec3 v, mu::Vec3 fallback)
{
    const float lenSq = mu::dot(v, v);
    if (lenSq < 1e-6f)
        return fallback;
    return v * (1.f / std::sqrt(lenSq));
}

}  // namespace

ParticleSystem& ParticleEffect::addSystem(const ps::ParticleSystemConfig& cfg,
                                          PlayMode mode,
                                          int maxParticles)
{
    auto& entry = systems_.emplace_back();
    entry.mode = mode;
    entry.shapeBasePosition = cfg.shape.position;
    entry.shapeBaseOrientation = cfg.shape.orientation;
    entry.meshBaseRotation = cfg.main.startRotation3D;
    entry.ps.init(cfg, maxParticles);
    return entry.ps;
}

void ParticleEffect::play(const mu::Vec3& pos)
{
    play(pos, mu::NQuat{});
}

void ParticleEffect::play(const mu::Vec3& pos, mu::NQuat orient)
{
    play(pos, mu::Mat4x4(orient));
}

void ParticleEffect::play(const mu::Vec3& pos, mu::Mat4x4 orientXform)
{
    const auto advanceForward = mu::Vec3(
        mu::Vec4(0.f, 0.f, 1.f, 0.f) * orientXform
    );
    play(pos, orientXform, advanceForward);
}

void ParticleEffect::play(const mu::Vec3& pos, mu::Mat4x4 orientXform, mu::Vec3 advanceForward)
{
    advanceForward_ = normalizedOr(advanceForward, { 0.f, 0.f, 1.f });
    for (auto& e : systems_) {
        const auto rotatedBasePosition = mu::Vec3(
            mu::Vec4(e.shapeBasePosition.x(), e.shapeBasePosition.y(), e.shapeBasePosition.z(), 0.f)
            * orientXform
        );
        e.ps.config().shape.position = pos + rotatedBasePosition;
        e.ps.config().shape.orientation = e.shapeBaseOrientation * orientXform;
        e.ps.config().main.startRotation3D = e.meshBaseRotation * orientXform;
        if (e.isSubEmitter) continue;
        if (e.mode == PlayMode::Emit)
            e.ps.emit(1);
        else
            e.ps.startContinuous();
    }
}

void ParticleEffect::stop()
{
    for (auto& e : systems_)
        e.ps.stopContinuous();
}

bool ParticleEffect::isAlive() const
{
    for (const auto& e : systems_)
        if (e.ps.activeCount() > 0)
            return true;
    return false;
}

void ParticleEffect::update(Seconds dt)
{
    // (1) Update all systems — parents emit SubEmitterEvents during this step.
    for (auto& e : systems_)
        e.ps.update(dt);

    // (2) For each parent event, create one PendingSubEmitterBurst per child burst entry.
    for (const auto& binding : subEmitterBindings_) {
        auto& parentPs     = systems_[binding.parentIdx].ps;
        auto& childPs      = systems_[binding.childIdx].ps;
        const auto& subCfg = parentPs.config().subEmitters.subEmitters[binding.subEmitterCfgIdx];

        for (const auto& evt : parentPs.pendingSubEmitterEvents()) {
            if (evt.configIndex != binding.subEmitterCfgIdx) continue;

            mu::Vec3 sourceVel = evt.vel;
            if (binding.flattenSourceVelocityY) {
                const float sourceSpeed = std::sqrt(mu::dot(sourceVel, sourceVel));
                sourceVel = { sourceVel.x(), 0.f, sourceVel.z() };
                sourceVel = normalizedOr(sourceVel, advanceForward_) * sourceSpeed;
            }

            const mu::Vec3 vel   = subCfg.inheritVelocity ? evt.vel   : mu::Vec3{ 0.f, 0.f, 0.f };
            const mu::Vec4 color = subCfg.inheritColor    ? evt.color : mu::Vec4{ 1.f, 1.f, 1.f, 1.f };
            const float    size  = subCfg.inheritSize     ? evt.size  : 1.f;
            const bool advanceWithSource = subCfg.event == ps::SubEmittersModule::Event::Birth;

            const auto& bursts = childPs.config().emission.bursts;
            for (int bi = 0; bi < (int)bursts.size(); ++bi) {
                pendingBursts_.push_back({
                    binding.childIdx,
                    bi,
                    evt.pos,
                    sourceVel,
                    advanceWithSource,
                    vel,
                    color,
                    size,
                    0.f,
                    0
                });
            }
        }
    }

    // Clear pending events after all bindings have dispatched — must happen after the full
    // dispatch loop so that multiple bindings sharing the same parent all get to read them.
    for (auto& e : systems_)
        e.ps.clearPendingSubEmitterEvents();

    // (3) Time-simulate each pending burst instance; fire cycles that are now due.
    const float dtf = dt.count();
    for (auto it = pendingBursts_.begin(); it != pendingBursts_.end(); ) {
        auto& pi      = *it;
        auto& childPs = systems_[pi.childIdx].ps;
        const auto& burst    = childPs.config().emission.bursts[pi.burstIdx];
        const float scaledDt = dtf * childPs.config().main.simulationSpeed;
        const float endTime  = pi.elapsedTime + scaledDt;

        while (pi.nextCycle < burst.cycleCount) {
            const float dueTime = burst.time + burst.repeatInterval * pi.nextCycle;
            if (dueTime >= endTime) break;

            if (std::uniform_real_distribution<float>{ 0.f, 1.f }(rng_) <= burst.probability) {
                const int lo = std::min(burst.countMin, burst.countMax);
                const int hi = std::max(burst.countMin, burst.countMax);
                const int n  = (lo == hi) ? lo
                             : std::uniform_int_distribution<int>{ lo, hi }(rng_);
                const mu::Vec3 burstPos = pi.advanceWithSource
                    ? pi.pos + pi.sourceVel * dueTime
                    : pi.pos;
                childPs.emitAt(n, burstPos, pi.inheritedVel, pi.inheritedColor, pi.inheritedSize);
            }
            ++pi.nextCycle;
        }

        pi.elapsedTime = endTime;

        if (pi.nextCycle >= burst.cycleCount)
            it = pendingBursts_.erase(it);
        else
            ++it;
    }
}

void ParticleEffect::bindSubEmitter(int parentIdx, int subEmitterCfgIdx, int childIdx,
                                    bool flattenSourceVelocityY)
{
    systems_[childIdx].isSubEmitter = true;
    subEmitterBindings_.push_back({ parentIdx, subEmitterCfgIdx, childIdx, flattenSourceVelocityY });
}

void ParticleEffect::render(GFX& gfx) const
{
    for (const auto& e : systems_)
        e.ps.render(gfx);
}
