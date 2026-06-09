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

-- Helper: ground attachment target. Plants OBBs at a caster-relative point snapped to
-- the terrain, then leaves them static (no bone tracking). opts:
--   align = true  -- tilt OBBs to the terrain normal (default false: yaw only, slope-safe)
--   rigid = true  -- point impact: snap ONCE at the anchor, place every OBB as a rigid
--                    offset from that single surface point (meteor explosion, blast crater).
--                    center.y becomes a vertical offset from the snapped anchor height.
--   rigid = false -- (default) each OBB snaps to the terrain at its own XZ -> a grid of
--                    pillars conforms to slopes independently (distributed eruption).
function GroundAttach(opts)
    opts = opts or {}
    return { type = "Ground", align = opts.align or false, rigid = opts.rigid or false }
end

-- PlayVFX event parameters (passed inline to skill:addEvent(ms, "PlayVFX", { ... })):
--   vfxId      = <int>             -- index into the runtime ParticleEffect registry (required)
--   offset     = Vec3(x, y, z)     -- attach-local placement (right, up, forward); forward = "N meters ahead"
--   orient     = { yaw, pitch, roll } -- attach-local orientation offset in degrees (same convention as OBB)
--                                     -- aims the effect relative to the caster (e.g. a fan to the side)
--   advance    = Vec3(x, y, z)     -- attach-local particle travel direction; omit to derive from orient
--   groundLock = true/false        -- place on the ground plane (ignore caster pitch/roll)
--   attach     = BoneAttach(name)  -- follow a bone; omit/empty = caster root
-- NOTE: the circle radius / fan radius+angle are authored in the effect prefab's shape config (.json),
--       not here. PlayVFX only controls placement, orientation, and travel direction.
-- Example -- forward 4m ground circle:   skill:addEvent(120,"PlayVFX",{ vfxId=7, offset=Vec3(0,0,4), groundLock=true })
-- Example -- 45-degree fan to the left:   skill:addEvent(120,"PlayVFX",{ vfxId=8, offset=Vec3(0,0,2), orient={45,0,0} })

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