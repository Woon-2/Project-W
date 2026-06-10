-- energy_explosion_arrow.lua
-- Foundation skill paired 1:1 with the "Energy Explosion Arrow" VFX (vfxId 13).
-- Projectile-into-burst archetype. Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "EnergyExplosionArrow"
skill.totalDurationMs  = 3400
skill.cooldownMs       = 4080
skill.interruptible    = true

skill:addVFX(13, "effects/energy_explosion_arrow.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(120, "PlayVFX", {
    vfxId  = 13,
    offset = Vec3(0.0, 1.0, 0.8)
})

local onHitBase = OnHit({
    damage          = 60,
    vfxId           = 255,
    impulseStrength = 800.0,
    impulseDir      = Vec3(0.0, 0.0, 0.0)
})

local radius    = 2.0
local count     = 8
local spawnMs   = 160
local destroyMs = 2800

skill:addEvent(spawnMs, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, 0.0, 0.0, 0.9, 1.4, 0.9, 0, 0, 0) },
    attach              = VFXParticleAttach(13, 2),
    applyAttachRotation = true,
    useParticleSize     = false,
    hitGroup            = 2,
    hitGroupCooldownMs  = 2400,
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
        localOBBs           = { OBB(0.75 * radius * s, 0.0, 0.75 * radius * c, 1.1, 1.4, 0.6, deg, 0, 0) },
        attach              = VFXParticleAttach(13, 2),
        hitGroup            = 2,
        hitGroupCooldownMs  = 2400,
        onHit               = onHit
    })
    skill:addEvent(destroyMs, "DestroyHitbox", { slot = i })
end

return skill
