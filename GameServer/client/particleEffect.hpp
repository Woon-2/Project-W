#ifndef __particleEffect_HPP
#define __particleEffect_HPP

#include "particleSystem.hpp"
#include <vector>

class GFX;

// Groups multiple ParticleSystem instances that together form one visual effect,
// mirroring a Unity prefab that contains several ParticleSystem child objects.
class ParticleEffect {
public:
    enum class PlayMode {
        Emit,        // calls emit(1) on play()
        Continuous,  // calls startContinuous() on play()
    };

    // Adds a sub-system and returns a reference for further inspection.
    ParticleSystem& addSystem(const ps::ParticleSystemConfig& cfg,
                              PlayMode mode,
                              int maxParticles = ParticleSystem::kDefaultMaxParticles);

    ParticleSystem&       system(int index)       { return systems_[index].ps; }
    const ParticleSystem& system(int index) const { return systems_[index].ps; }
    int systemCount() const { return static_cast<int>(systems_.size()); }

    // Sets shape.position on all sub-systems and triggers each per its PlayMode.
    void play(const mu::Vec3& pos);

    // Calls stopContinuous() on all sub-systems.
    void stop();

    // True if any sub-system has active particles.
    bool isAlive() const;

    void update(Seconds dt);
    void render(GFX& gfx) const;

private:
    struct Entry {
        ParticleSystem ps;
        PlayMode       mode;
    };
    std::vector<Entry> systems_;
};

#endif  // __particleEffect_HPP
