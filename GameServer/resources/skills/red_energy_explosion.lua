-- red_energy_explosion.lua
-- Foundation skill paired 1:1 with the "Red Energy Explosion" VFX (vfxId 8).
-- Meteor archetype: an orb falls and detonates on the ground in front of the caster.
-- Point-impact ground anchor keeps the blast hitboxes coherent with the single-origin
-- explosion VFX on sloped terrain. Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "RedEnergyExplosion"
skill.totalDurationMs  = 2000
skill.interruptible    = true

skill:addVFX(8, "effects/red_energy_explosion.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

-- Impact point: 5m in front of the caster, dropped onto the terrain surface.
-- The explosion VFX and every blast hitbox share this exact point.
local impact = Vec3(0.0, 0.0, 5.0)

skill:addEvent(150, "PlayVFX", {
    vfxId  = 8,
    offset = impact,
    groundSnap = true,
    particleCollision = "GroundStop"
})

local onHitBase = OnHit({
    damage          = 40,
    vfxId           = 255,
    impulseStrength = 900.0,
    impulseDir      = Vec3(0.0, 1.0, 0.0)
})

-- Ring layout. Anchor-frame axes: right = X, up = Y, forward = Z (relative to the
-- registered impact point, NOT the caster). Each box sits on a circle of radius
-- 0.75 * `radius` at angle `deg`, faces outward, and pushes enemies radially out.
local radius    = 2.2
local count     = 8
local anchorMs  = 1150   -- register the impact anchor BEFORE the hitboxes reference it
local spawnMs   = 1200
local destroyMs = 1450

-- Register the shared point of impact (anchor slot 0). Hitboxes below reference it
-- via GroundAttach{ anchor = 0 }, so each keeps its own onHit / radial knockback
-- while all sharing one terrain-snapped origin.
skill:addEvent(anchorMs, "SetGroundAnchor", GroundAnchor({ id = 0, offset = impact, align = false }))

-- Central box at the impact point.
skill:addEvent(spawnMs, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0, 0, 0, 1.1, 1.2, 1.1, 0, 0, 0) },
    attach              = GroundAttach({ anchor = 0 }),
    hitGroup            = 0,
    hitGroupCooldownMs  = 600,
    onHit               = onHitBase
})
skill:addEvent(destroyMs, "DestroyHitbox", { slot = 0 })

for i = 1, count do
    local deg = i * (360.0 / count)
    local rad = math.rad(deg)
    local s   = math.sin(rad)
    local c   = math.cos(rad)

    local onHit = deepCopy(onHitBase)
    onHit.impulseDir = Vec3(s, 0.0, c)

    skill:addEvent(spawnMs, "SpawnHitbox", {
        slot                = i,
        -- center on the ring (anchor-frame offset); halfExtents: tangential X, height Y, radial Z.
        localOBBs           = { OBB(0.75 * radius * s, 0.0, 0.75 * radius * c, 1.1, 1.2, 1.1, deg, 0, 0) },
        attach              = GroundAttach({ anchor = 0 }),
        hitGroup            = 0,
        hitGroupCooldownMs  = 600,
        onHit               = onHit
    })
    skill:addEvent(destroyMs, "DestroyHitbox", { slot = i })
end

return skill
