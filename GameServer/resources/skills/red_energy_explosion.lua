-- red_energy_explosion.lua
-- Foundation skill paired 1:1 with the "Red Energy Explosion" VFX (vfxId 8).
-- Ground-AoE archetype (burst around caster). Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "RedEnergyExplosion"
skill.totalDurationMs  = 2000
skill.interruptible    = true

skill:addVFX(8, "effects/red_energy_explosion.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(150, "PlayVFX", {
    vfxId  = 8,
    offset = Vec3(0.0, 0.0, 5.0),
    groundSnap = true,
    particleCollision = "GroundStop"
})

local onHitBase = OnHit({
    damage          = 40,
    vfxId           = 255,
    impulseStrength = 900.0,
    impulseDir      = Vec3(0.0, 1.0, 0.0)
})

-- Ring layout. attach-local axes: right = X, up = Y, forward = Z.
-- The i-th box sits on a circle of radius `radius` at angle `deg` measured
-- around the up axis from the forward direction, oriented to face outward,
-- and pushes enemies radially outward with a slight upward lift.
local radius    = 2.2
local count     = 8
local spawnMs   = 1200
local destroyMs = 1450

skill:addEvent(spawnMs, "SpawnHitbox", {
    slot = count,
    localOBBs = { OBB(0, 0, 5.0, 1.1, 1.2, 1.1, 0, 0, 0)},
    attach              = GroundAttach( { align = false, rigid = false } ),
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
        -- center on the ring; halfExtents: tangential X, height Y, radial Z.
        localOBBs           = { OBB(0.75 * radius * s, 0.0, 5.0 + 0.75 * radius * c, 1.1, 1.2, 1.1, deg, 0, 0) },
        attach              = GroundAttach( { align = false, rigid = true } ),
        hitGroup            = 0,
        hitGroupCooldownMs  = 600,
        onHit               = onHit
    })
    skill:addEvent(destroyMs, "DestroyHitbox", { slot = i })
end

return skill
