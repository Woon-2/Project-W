#include "pch.hpp"
#include "particleEffect.hpp"

ParticleSystem& ParticleEffect::addSystem(const ps::ParticleSystemConfig& cfg,
                                          PlayMode mode,
                                          int maxParticles)
{
    auto& entry = systems_.emplace_back();
    entry.mode = mode;
    entry.shapeBasePosition = cfg.shape.position;
    entry.ps.init(cfg, maxParticles);
    return entry.ps;
}

void ParticleEffect::play(const mu::Vec3& pos)
{
    for (auto& e : systems_) {
        e.ps.config().shape.position = pos + e.shapeBasePosition;
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
    for (auto& e : systems_)
        e.ps.update(dt);
}

void ParticleEffect::render(GFX& gfx) const
{
    for (const auto& e : systems_)
        e.ps.render(gfx);
}
