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
    offset = Vec3(0.0, 1.0, 0.8)
})

local onHit = OnHit({
    damage          = 35,
    vfxId           = 255,
    impulseStrength = 500.0,
    impulseDir      = Vec3(0.0, 0.1, 1.0)
})

skill:addEvent(140, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, -0.2, 2.5, 0.4, 0.4, 2.5, 0, 0, 0) },
    attach              = BoneAttach("spine_02"),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 600,
    onHit               = onHit
})

skill:addEvent(400, "DestroyHitbox", { slot = 0 })

return skill
