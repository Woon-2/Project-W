-- energy_explosion_arrow.lua
-- Foundation skill paired 1:1 with the "Energy Explosion Arrow" VFX (vfxId 13).
-- Projectile-into-burst archetype. Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "EnergyExplosionArrow"
skill.weapon           = "bow"
skill.dialSlot         = 2
skill.chargeCost       = 9
skill.cooldownMs       = 3800
skill.totalDurationMs  = 3400
skill.interruptible    = true

-- systems[]: code-built composition (game.cpp). Sub-emitter chain:
-- Charge(0, root) -Death-> Arrow(1) -Death-> Hit(2). All params constant, so
-- the server's analytic chain lands exactly where the client explosion plays.
skill:addVFX(13, "effects/energy_explosion_arrow.json", {
    systems = {
        {   -- 0: Charge (root; PlayMode Emit)
            mode = "Emit", looping = false,
            lifetime = 1.6, speed = 0.0, startSize = 8.0,
            shapeType = "Point", maxParticles = 16,
        },
        {   -- 1: Arrow (chained: Charge death)
            parent = 0, parentEvent = "Death", looping = false,
            lifetime = 0.6, speed = 40.0, startSize = 0.3,
            shapeType = "Point", direction = Vec3(0.0, 0.0, 1.0),
            maxParticles = 32,
            bursts = { { time = 0.0, count = 1, cycleCount = 1 } },
        },
        {   -- 2: Hit explosion (chained: Arrow death) -- hitbox target
            parent = 1, parentEvent = "Death", looping = false,
            lifetime = 1.6, speed = 0.0, startSize = 10.0,
            shapeType = "Point", maxParticles = 16,
            bursts = { { time = 0.0, count = 1, cycleCount = 1 } },
        },
    }
})

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
