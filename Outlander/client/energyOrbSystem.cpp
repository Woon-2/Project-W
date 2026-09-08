#include "pch.hpp"
#include "energyOrbSystem.hpp"
#include "gfx.hpp"
#include "mesh.hpp"
#include <algorithm>
#include <cmath>

namespace {

// Lifecycle timing / motion knobs.
// The orb phase is deliberately kept SHORT: the ragdoll hold before it is the part
// worth watching, so time given back here buys ragdoll screen time at a constant
// total (death -> fully absorbed ~= 3.6s -- budget table in docs/gameArchitecture.md).
// kFormingTime dominates the felt delay because the orb is completely stationary for
// its whole duration, so shorten it before touching the speeds.
constexpr float kFormingTime  = 0.90f;  // seconds for vertices -> sphere
constexpr float kStartSpeed   = 8.0f;   // tracking speed at Forming->Tracking
constexpr float kAccel        = 45.f;   // tracking acceleration (m/s^2)
constexpr float kMaxSpeed     = 34.f;   // tracking max speed (m/s)
constexpr float kAbsorbRadius = 0.7f;   // distance to player center that counts as a hit
constexpr float kAbsorbTime   = 0.18f;  // Absorbing state duration
constexpr float kTargetHeight = 1.1f;   // aim at the player's chest, not feet
constexpr float kMaxOrbLifetime = 8.0f; // failsafe: force-absorb a still-tracking orb past
                                        // this age so it can never linger (e.g. a player
                                        // fleeing faster than kMaxSpeed) and leave its owning
                                        // corpse stuck in corpses_, never returned to the pool

constexpr float kSphereRadius = 0.32f;
constexpr float kPointSize    = 0.04f;
constexpr float kColorMinI    = 1.6f;   // HDR intensity range (kept modest: hundreds of
constexpr float kColorMaxI    = 2.7f;   // additive quads stack into a bright core already)

// Condense the orb as it nears the player. Without this the fixed-world-size quads
// swell on screen via perspective (the player sits near the camera) and the dense
// additive core blooms into a big blob. Shrinking the world size as it approaches
// counters the swell and reads as energy compressing into the body.
constexpr float kCondenseStartDist = 3.5f;  // begin shrinking within this range of the player
constexpr float kCondenseMinScale  = 0.5f;  // world-size scale at contact

// Fully-saturated hue -> RGB (h in [0,1)).
mu::Vec3 hueToRGB(float h) {
    const float r = std::clamp(std::abs(h * 6.f - 3.f) - 1.f, 0.f, 1.f);
    const float g = std::clamp(2.f - std::abs(h * 6.f - 2.f), 0.f, 1.f);
    const float b = std::clamp(2.f - std::abs(h * 6.f - 4.f), 0.f, 1.f);
    return mu::Vec3{ r, g, b };
}

}  // namespace

void EnergyOrbSystem::spawnFromMonster(const Model& model,
                                       std::span<const mu::Mat4x4> finalXforms,
                                       const mu::Mat4x4& objWorld,
                                       float totalCharge, int slot, u32t corpseId) {
    // Count skinned submeshes first so charge can be split evenly.
    int orbCount = 0;
    for (const auto& mwd : model.meshWithDressXforms) {
        const Mesh& mesh = mwd.mesh;
        if (!mesh.vbIdxMap.contains(mesh.name + "_VB_BoneIndices")) continue;
        orbCount += static_cast<int>(mesh.subMeshes.size());
    }
    if (orbCount == 0) return;

    const float chargePerOrb = totalCharge / static_cast<float>(orbCount);

    // Monster root world position (orbs converge around the corpse center).
    const mu::Vec3 rootW = mu::Vec3(mu::Vec4(mu::Vec3{ 0.f, 0.f, 0.f }, 1.f) * objWorld);

    for (const auto& mwd : model.meshWithDressXforms) {
        const Mesh& mesh = mwd.mesh;
        if (!mesh.vbIdxMap.contains(mesh.name + "_VB_BoneIndices")) continue;

        for (size_t si = 0; si < mesh.subMeshes.size(); ++si) {
            const SubMesh& subMesh = mesh.subMeshes[si];
            Orb orb;
            orb.mesh    = &mesh;
            orb.subMesh = &subMesh;
            if (!mesh.materialSets.empty() && si < mesh.materialSets[0].materials.size())
                orb.albedo = &mesh.materialSets[0].materials[si].mapAlbedo;
            orb.boneSnapshot.assign(finalXforms.begin(), finalXforms.end());
            orb.objWorldAtDeath = objWorld;
            // Sphere center = this submesh's first vertex skinned by the death pose, so
            // each submesh collapses to its own world point (corpse breaks into pieces).
            mu::Vec3 center = rootW + mu::Vec3{ 0.f, kTargetHeight, 0.f };  // fallback
            if (subMesh.hasFirstVertex) {
                const mu::Vec3 localPos = mu::Vec3(DirectX::XMLoadFloat3(&subMesh.firstVertexPos));
                const int   bi[4] = { subMesh.firstVertexBones.x, subMesh.firstVertexBones.y,
                                      subMesh.firstVertexBones.z, subMesh.firstVertexBones.w };
                const float bw[4] = { subMesh.firstVertexWeights.x, subMesh.firstVertexWeights.y,
                                      subMesh.firstVertexWeights.z, subMesh.firstVertexWeights.w };
                mu::Vec3 dressPos{ 0.f, 0.f, 0.f };
                float wsum = 0.f;
                for (int k = 0; k < 4; ++k) {
                    if (bw[k] <= 0.f) continue;
                    if (bi[k] < 0 || static_cast<size_t>(bi[k]) >= finalXforms.size()) continue;
                    dressPos = dressPos + mu::Vec3(mu::Vec4(localPos, 1.f) * finalXforms[bi[k]]) * bw[k];
                    wsum += bw[k];
                }
                if (wsum > 1e-4f)
                    center = mu::Vec3(mu::Vec4(dressPos * (1.f / wsum), 1.f) * objWorld);
            }
            orb.sphereCenter = center;
            orb.sphereRadius = kSphereRadius;
            orb.colorHDR     = hueToRGB(rand(0.f, 1.f)) * rand(kColorMinI, kColorMaxI);
            orb.pointSize    = kPointSize;
            orb.vertexCount  = subMesh.ibView.SizeInBytes / static_cast<u32t>(sizeof(u16t));
            orb.chargePerOrb = chargePerOrb;
            orb.slot         = slot;
            orb.corpseId     = corpseId;
            orb.morphT       = 0.f;
            orb.speed        = 0.f;
            orb.state        = State::Forming;
            orb.stateTime    = 0.f;
            orbs_.push_back(std::move(orb));
        }
    }
}

bool EnergyOrbSystem::hasActiveOrbs(u32t corpseId) const {
    for (const auto& orb : orbs_)
        if (orb.corpseId == corpseId && orb.state != State::Dead) return true;
    return false;
}

void EnergyOrbSystem::update(float dtSec, const mu::Vec3& playerPos) {
    const mu::Vec3 target = playerPos + mu::Vec3{ 0.f, kTargetHeight, 0.f };

    for (auto& orb : orbs_) {
        orb.stateTime += dtSec;
        orb.age       += dtSec;
        switch (orb.state) {
        case State::Forming: {
            orb.morphT = std::clamp(orb.stateTime / kFormingTime, 0.f, 1.f);
            if (orb.stateTime >= kFormingTime) {
                orb.morphT    = 1.f;
                orb.speed     = kStartSpeed;
                orb.state     = State::Tracking;
                orb.stateTime = 0.f;
            }
            break;
        }
        case State::Tracking: {
            orb.speed = std::min(orb.speed + kAccel * dtSec, kMaxSpeed);
            mu::Vec3 toTarget = target - orb.sphereCenter;
            const float dist = toTarget.len();
            // Condense as it approaches (counter perspective swell + read as compression).
            const float u = std::clamp((dist - kAbsorbRadius) / (kCondenseStartDist - kAbsorbRadius), 0.f, 1.f);
            orb.renderScale = kCondenseMinScale + (1.f - kCondenseMinScale) * u;
            // Absorb on contact, or force-absorb past the lifetime failsafe (so the orb — and
            // the corpse waiting on it — can never get stuck if the player outruns it).
            if (dist <= kAbsorbRadius || orb.age >= kMaxOrbLifetime) {
                orb.contactPoint = (dist <= kAbsorbRadius) ? orb.sphereCenter : target;
                orb.state        = State::Absorbing;
                orb.stateTime    = 0.f;
                if (onAbsorb) onAbsorb(orb);
            } else {
                const mu::Vec3 dir = toTarget * (1.f / dist);
                orb.sphereCenter = orb.sphereCenter + dir * (orb.speed * dtSec);
            }
            break;
        }
        case State::Absorbing: {
            // Shrink to nothing so the orb visibly pops into the body rather than
            // lingering as a large quad at the contact point.
            orb.renderScale = kCondenseMinScale * std::max(0.f, 1.f - orb.stateTime / kAbsorbTime);
            if (orb.stateTime >= kAbsorbTime) orb.state = State::Dead;
            break;
        }
        case State::Dead:
            break;
        }
    }

    std::erase_if(orbs_, [](const Orb& o) { return o.state == State::Dead; });
}

void EnergyOrbSystem::submitDrawEvents(GFX& gfx) const {
    for (const auto& orb : orbs_) {
        if (orb.state == State::Dead) continue;
        gfx.addDrawEvent(EnergyOrbPipeline::DrawEvent{
            .world        = orb.objWorldAtDeath,
            .pMesh        = orb.mesh,
            .pSubMesh     = orb.subMesh,
            .pAlbedo      = orb.albedo,
            .boneXforms   = std::span<const mu::Mat4x4>(orb.boneSnapshot),
            .sphereCenter = orb.sphereCenter,
            .sphereRadius = orb.sphereRadius * orb.renderScale,
            .colorHDR     = orb.colorHDR,
            .morphT       = orb.morphT,
            .pointSize    = orb.pointSize * orb.renderScale,
            .vertexCount  = orb.vertexCount,
        });
    }
}
