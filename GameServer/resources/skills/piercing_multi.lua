-- piercing_multi.lua
-- Foundation skill paired 1:1 with the "Piercing Multi" VFX (vfxId 18).
-- Multi-projectile archetype (wide forward box). Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "PiercingMulti"
skill.totalDurationMs  = 700
skill.interruptible    = true

skill:addVFX(18, "effects/piercing_multi.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(100, "PlayVFX", {
    vfxId  = 18,
    offset = Vec3(0.0, 1.0, 0.8)
})

local onHit = OnHit({
    damage          = 30,
    vfxId           = 255,
    impulseStrength = 500.0,
    impulseDir      = Vec3(0.0, 0.1, 1.0)
})

skill:addEvent(130, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, -0.2, 2.8, 1.4, 0.6, 2.8, 0, 0, 0) },
    attach              = BoneAttach("spine_02"),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 600,
    onHit               = onHit
})

skill:addEvent(500, "DestroyHitbox", { slot = 0 })

return skill
