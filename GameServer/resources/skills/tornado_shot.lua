-- tornado_shot.lua
-- Foundation skill paired 1:1 with the "Tornado Shot" VFX (vfxId 14).
-- Travelling projectile archetype. Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "TornadoShot"
skill.totalDurationMs  = 1000
skill.interruptible    = true

skill:addVFX(14, "effects/tornado_shot.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(120, "PlayVFX", {
    vfxId  = 14,
    offset = Vec3(0.0, 1.0, 1.0)
})

local onHit = OnHit({
    damage          = 40,
    vfxId           = 255,
    impulseStrength = 600.0,
    impulseDir      = Vec3(0.0, 0.4, 1.0)
})

skill:addEvent(150, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, -0.2, 2.5, 0.9, 1.2, 2.5, 0, 0, 0) },
    attach              = BoneAttach("spine_02"),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 600,
    onHit               = onHit
})

skill:addEvent(800, "DestroyHitbox", { slot = 0 })

return skill
