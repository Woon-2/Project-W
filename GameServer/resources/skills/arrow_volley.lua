-- arrow_volley.lua
-- Foundation skill paired 1:1 with the "Arrow Volley" VFX (vfxId 11).
-- Projectile archetype (wide forward fan). Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "ArrowVolley"
skill.totalDurationMs  = 700
skill.interruptible    = true

skill:addVFX(11, "effects/arrow_volley.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(120, "PlayVFX", {
    vfxId  = 11,
    offset = Vec3(0.0, 1.0, 0.8)
})

local onHit = OnHit({
    damage          = 28,
    vfxId           = 255,
    impulseStrength = 400.0,
    impulseDir      = Vec3(0.0, 0.1, 1.0)
})

skill:addEvent(140, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, -0.2, 3.0, 1.6, 0.5, 3.0, 0, 0, 0) },
    attach              = BoneAttach("spine_02"),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 600,
    onHit               = onHit
})

skill:addEvent(500, "DestroyHitbox", { slot = 0 })

return skill
