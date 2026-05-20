-- skill_api.lua
-- Lua-side helper utilities for skill definition files.
-- Loaded automatically by SkillCompiler before executing individual skill files.

-- Helper: create a Vec3 table with named fields x,y,z and positional [1],[2],[3].
function Vec3(x, y, z)
    return { x, y, z, x = x, y = y, z = z }
end

-- Helper: create an OBB table { center = Vec3, halfExtents = Vec3 }.
-- Used in the localOBBs array of SpawnHitbox events.
function OBB(cx, cy, cz, hex, hey, hez)
    return {
        center      = Vec3(cx, cy, cz),
        halfExtents = Vec3(hex, hey, hez)
    }
end

-- Helper: bone attachment target.
function BoneAttach(boneName)
    return { type = "Bone", name = boneName }
end

-- Helper: VFX particle system attachment.
-- All active particles in ParticleEffect[vfxId].system(systemIdx) receive hitboxes.
function VFXParticleAttach(vfxId, systemIdx)
    return { type = "VFXParticle", vfxId = vfxId, systemIdx = systemIdx or 0 }
end

-- Helper: on-hit response definition.
function OnHit(params)
    return {
        damage          = params.damage          or 0,
        vfxId           = params.vfxId           or 255,  -- 255 = no VFX
        impulseStrength = params.impulseStrength  or 0.0,
        impulseDir      = params.impulseDir       or Vec3(0, 0, 1)
    }
end
