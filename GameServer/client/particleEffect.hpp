#ifndef __particleEffect_HPP
#define __particleEffect_HPP

#include "particleSystem.hpp"
#include <vector>
#include <random>
#include <functional>

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

    // Binds a terrain query used by ground-conform spawn and ground collision on
    // every sub-system (including ones added later). Non-owning; nullptr clears.
    void setGroundSampler(const GroundSampler* g);

    // Skill-driven (Lua PlayVFX) ground behaviour. Overrides the played effect's
    // particle config so a skill can make an effect's particles collide with /
    // conform to the terrain WITHOUT touching the Unity-exported JSON. Applied to
    // top-level systems only (sub-emitter children, e.g. impact bursts, are left
    // alone so they are not killed the instant they spawn at the contact point).
    // Mode::None / GroundConform::None leave the respective behaviour unchanged.
    void setGroundBehavior(ps::ParticleCollisionModule::Mode collision,
                           ps::ShapeModule::GroundConform     conform);

    // Per-cast deterministic seed (set by the skill system before play()).
    // Forwarded to each non-sub-emitter system as mixSeed(seed, systemIndex);
    // systems without a gameplay config ignore it. The server derives the
    // same per-system seeds (see RoomServer SkillSystem particle sources).
    void setDeterministicSeed(std::uint32_t seed);

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

    // Cosmetic hook fired when a sub-emitter child system spawns, at the spawn
    // position (world). Fired once per parent trigger event (not per burst), so a
    // multi-burst child plays the hook exactly once. Lets the skill glue play
    // launch/impact SFX without coupling the VFX layer to the sound system.
    // Null = no-op (default).
    void setChildSpawnCallback(std::function<void(int childIdx, const mu::Vec3& pos)> cb) {
        onChildSpawn_ = std::move(cb);
    }

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
    std::function<void(int childIdx, const mu::Vec3& pos)> onChildSpawn_;  // optional spawn hook
    std::mt19937                        rng_{ std::random_device{}() };
    mu::Vec3                            advanceForward_ = { 0.f, 0.f, 1.f };
    const GroundSampler*                ground_ = nullptr;  // non-owning terrain query (optional)
};

#endif  // __particleEffect_HPP
