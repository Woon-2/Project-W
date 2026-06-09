-- arrow.lua
-- Foundation skill paired 1:1 with the "Arrow" VFX (vfxId 10).
-- Projectile archetype (long forward box). Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "Arrow"
skill.totalDurationMs  = 600
skill.interruptible    = true

skill:addVFX(10, "effects/arrow.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(120, "PlayVFX", {
    vfxId  = 10,
    offset = Vec3(0.0, 1.0, 0.8),
    advance = Vec3(0.0, 0.0, 1.0)
})

local onHit = OnHit({
    damage          = 10,
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
    hitGroup            = 0,
    hitGroupCooldownMs  = 550,
    onHit               = onHit
})

skill:addEvent(580, "DestroyHitbox", { slot = 0 })

return skill
