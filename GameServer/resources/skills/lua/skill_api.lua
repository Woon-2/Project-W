-- skill_api.lua
-- Lua-side helper utilities for skill definition files.
-- Loaded automatically by SkillCompiler before executing individual skill files.

-- Helper: create a Vec3 table with named fields x,y,z and positional [1],[2],[3].
function Vec3(x, y, z)
    return { x, y, z, x = x, y = y, z = z }
end

-- Helper: create an OBB table { center = Vec3, halfExtents = Vec3 [, orient = {yaw,pitch,roll}] }.
-- Used in the localOBBs array of SpawnHitbox events.
-- Optional yaw/pitch/roll are Euler angles in degrees (yaw=Y-axis, pitch=X-axis, roll=Z-axis).
-- When omitted the OBB has identity orientation.
function OBB(cx, cy, cz, hex, hey, hez, yaw, pitch, roll)
    local t = {
        center      = Vec3(cx, cy, cz),
        halfExtents = Vec3(hex, hey, hez)
    }
    if yaw ~= nil or pitch ~= nil or roll ~= nil then
        t.orient = { yaw or 0, pitch or 0, roll or 0 }
    end
    return t
end

-- Helper: bone attachment target.
function BoneAttach(boneName)
    return { type = "Bone", name = boneName }
end

-- Helper: VFX particle system attachment.
-- All active particles in ParticleEffect[vfxId].system(systemIdx) receive hitboxes.
-- When useParticleSize = true in the SpawnHitbox event, each particle's current visual size
-- scales the OBB halfExtents defined in localOBBs (size=1.0 → halfExtents unchanged).
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

function deepCopy(orig)
    local origType = type(orig)
    local copy

    if origType == "table" then
        copy = {}
        for key, value in next, orig, nil do
            copy[deepCopy(key)] = deepCopy(value)
        end

        setmetatable(copy, deepCopy(getmetatable(orig)))
    else
        copy = orig
    end

    return copy
end