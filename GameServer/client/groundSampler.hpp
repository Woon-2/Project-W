#ifndef __groundSampler_HPP
#define __groundSampler_HPP

// Injected terrain query shared by the GFX particle layer and the skill dispatch
// layer. Both layers depend ONLY on this lightweight callback bundle, never on
// TerrainChunkManager, preserving the GFX/game layer separation.
//
// The game layer (standalone/online Game) binds the callbacks to
// TerrainChunkManager::heightAtWorld / normalAtWorld. An unbound sampler is
// treated as "no terrain": height() returns 0 and normal() returns world-up.
// Callers should test operator bool() and skip ground snapping when it is false
// (e.g. terrain chunk not loaded), rather than snapping everything to y = 0.
//
// mu::Vec3 is provided transitively by the precompiled header (matching the rest
// of the client headers, e.g. collision.hpp).

#include <functional>

struct GroundSampler {
    std::function<float(float x, float z)>    heightAt;
    std::function<mu::Vec3(float x, float z)> normalAt;

    float    height(float x, float z) const {
        return heightAt ? heightAt(x, z) : 0.f;
    }
    mu::Vec3 normal(float x, float z) const {
        return normalAt ? normalAt(x, z) : mu::Vec3{ 0.f, 1.f, 0.f };
    }

    explicit operator bool() const { return static_cast<bool>(heightAt); }
};

#endif  // __groundSampler_HPP
