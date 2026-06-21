#ifndef __energyOrbSystem_HPP
#define __energyOrbSystem_HPP

#include "mathUtil.hpp"
#include <vector>
#include <span>
#include <functional>

class GFX;
struct Model;
struct Mesh;
struct SubMesh;
struct Texture;

// Manages the lifecycle of monster-death energy orbs (mode-agnostic: depends only
// on GFX + a player position + an absorb callback, so both online and standalone
// can drive it).
//
// One orb per submesh: at spawn the orb captures a deep copy of the death-pose
// bone palette so EnergyOrbPipeline can skin + morph the submesh vertices into a
// glowing sphere (Forming), which then tracks the player (Tracking) and is
// absorbed on contact (Absorbing -> Dead). Charge crediting + the player ripple
// FX are delegated to onAbsorb so this stays game-layer-agnostic.
class EnergyOrbSystem {
public:
    enum class State { Forming, Tracking, Absorbing, Dead };

    struct Orb {
        const Mesh*    mesh    = nullptr;
        const SubMesh* subMesh = nullptr;
        const Texture* albedo  = nullptr;      // submesh albedo (morph start color)
        std::vector<mu::Mat4x4> boneSnapshot;  // death-pose palette (owned)
        mu::Mat4x4 objWorldAtDeath;
        mu::Vec3   sphereCenter  = { 0.f, 0.f, 0.f };
        float      sphereRadius  = 0.35f;
        mu::Vec3   colorHDR      = { 1.f, 1.f, 1.f };
        float      pointSize     = 0.04f;
        u32t       vertexCount   = 0u;
        float      chargePerOrb  = 0.f;
        int        slot          = 0;
        float      morphT        = 0.f;        // 0 = mesh pose, 1 = collapsed sphere
        float      speed         = 0.f;        // current tracking speed (m/s)
        float      renderScale   = 1.f;        // world-size multiplier (condense as it nears the player)
        mu::Vec3   contactPoint  = { 0.f, 0.f, 0.f };  // world pos at absorption (ripple anchor)
        u32t       corpseId      = 0u;         // owning corpse (so the corpse knows when its orbs are gone)
        State      state         = State::Forming;
        float      stateTime     = 0.f;      // time in the current state
        float      age           = 0.f;      // total lifetime (failsafe so an orb that can't
                                             // catch the player is force-absorbed, never leaks)
    };

    // Spawns one orb per submesh of the monster model, using the death-pose final
    // bone transforms (deep-copied). totalCharge is split evenly across the orbs.
    // corpseId tags every spawned orb so the owning corpse can detect completion.
    void spawnFromMonster(const Model& model,
                          std::span<const mu::Mat4x4> finalXforms,
                          const mu::Mat4x4& objWorld,
                          float totalCharge, int slot, u32t corpseId = 0u);

    // True while at least one (non-dead) orb belonging to corpseId is still alive.
    bool hasActiveOrbs(u32t corpseId) const;

    // Advances all orbs. playerPos is the live local-player world position.
    void update(float dtSec, const mu::Vec3& playerPos);

    // Submits one EnergyOrbPipeline::DrawEvent per live orb.
    void submitDrawEvents(GFX& gfx) const;

    bool empty() const { return orbs_.empty(); }
    void clear() { orbs_.clear(); }
    std::size_t orbCount() const { return orbs_.size(); }

    // Invoked once per orb when it reaches the player (contactPoint/colorHDR/
    // chargePerOrb/slot valid). Used to credit charge + trigger the body ripple.
    std::function<void(const Orb&)> onAbsorb;

private:
    std::vector<Orb> orbs_;
};

#endif  // __energyOrbSystem_HPP
