#ifndef __particleEffect_HPP
#define __particleEffect_HPP

#include "particleSystem.hpp"
#include <vector>
#include <random>

class GFX;

struct SubEmitterBinding {
    int parentIdx;
    int subEmitterCfgIdx;  // index into parent.config().subEmitters.subEmitters
    int childIdx;
    bool flattenSourceVelocityY = false;
};

// One active burst-sequence instance: one parent trigger event → one child burst entry replayed.
struct PendingSubEmitterBurst {
    int       childIdx;
    int       burstIdx;        // child.config().emission.bursts[burstIdx]
    mu::Vec3  pos;
    mu::Vec3  sourceVel;
    bool      advanceWithSource = false;
    mu::Vec3  inheritedVel;
    mu::Vec4  inheritedColor;
    float     inheritedSize;
    float     elapsedTime = 0.f;
    int       nextCycle   = 0;
};

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
    void play(const mu::Vec3& pos, mu::NQuat orient);
    void play(const mu::Vec3& pos, mu::Mat4x4 orientXform);
    void play(const mu::Vec3& pos, mu::Mat4x4 orientXform, mu::Vec3 advanceForward);

    // Updates emission origin for all sub-systems without restarting emission.
    // Call each frame while a continuous effect is in flight.
    void setOrigin(const mu::Vec3& pos, mu::NQuat orient);
    void setOrigin(const mu::Vec3& pos, mu::Mat4x4 orientXform);

    // Calls stopContinuous() on all sub-systems.
    void stop();

    // True if any sub-system has active particles.
    bool isAlive() const;

    void update(Seconds dt);
    void render(GFX& gfx) const;

    // Marks systems_[childIdx] as a sub-emitter driven by systems_[parentIdx].
    // Sub-emitter systems are skipped by play() and spawned via the parent's SubEmitterEvents.
    void bindSubEmitter(int parentIdx, int subEmitterCfgIdx, int childIdx,
                        bool flattenSourceVelocityY = false);

private:
    struct Entry {
        ParticleSystem ps;
        PlayMode       mode;
        bool           isSubEmitter = false;
        mu::Vec3       shapeBasePosition = { 0.f, 0.f, 0.f };
        mu::Mat4x4     shapeBaseOrientation = {};
        mu::Mat4x4     meshBaseRotation = {};
    };
    std::vector<Entry>                  systems_;
    std::vector<SubEmitterBinding>      subEmitterBindings_;
    std::vector<PendingSubEmitterBurst> pendingBursts_;
    std::mt19937                        rng_{ std::random_device{}() };
    mu::Vec3                            advanceForward_ = { 0.f, 0.f, 1.f };
};

#endif  // __particleEffect_HPP
