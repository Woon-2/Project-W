-- arrow.lua
-- Foundation skill paired 1:1 with the "Arrow" VFX (vfxId 10).
-- Projectile archetype (long forward box). Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "Arrow"
skill.weapon           = "bow"
skill.isBasic          = true
skill.totalDurationMs  = 600
skill.interruptible    = true

-- systems[1] = arrow mesh (code-built in game.cpp; no JSON source -> name omitted).
-- Overrides mirror the client cfg so server hitboxes track the same projectile.
skill:addVFX(10, "effects/arrow.json", {
    systems = {
        {
            mode = "Emit", looping = false,
            lifetime = 0.4, speed = 40.0, startSize = 0.3,
            shapeType = "Point", direction = Vec3(0.0, 0.0, 1.0),
            maxParticles = 32,
        },
    }
})

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Combat_Bow_Attack",
    attackIndex = 0,
    blendTime   = 0.1
})

skill:addEvent(120, "PlayVFX", {
    vfxId  = 10,
    offset = Vec3(0.0, 1.0, 0.8),
    advance = Vec3(0.0, 0.0, 1.0)
})

-- Bow release SFX, on the shot.
skill:addEvent(120, "PlaySound", { sound = "arrow_default" })

local onHit = OnHit({
    damage          = 50,
    vfxId           = 255,
    impulseStrength = 350.0,
    impulseDir      = Vec3(0.0, 0.0, 1.0)
})

skill:addEvent(130, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, 0.0, -0.3, 0.25, 0.25, 0.6, 0, 0, 0) },
    attach              = VFXParticleAttach(10, 0),
    applyAttachRotation = true,
    useParticleSize     = false,
    penetrate           = false,   -- non-penetrating: arrow particle is destroyed on first hit
    hitGroup            = 0,
    hitGroupCooldownMs  = 550,
    onHit               = onHit
})

skill:addEvent(580, "DestroyHitbox", { slot = 0 })

return skill
