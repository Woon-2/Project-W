-- piercing.lua
-- Foundation skill paired 1:1 with the "Piercing" VFX (vfxId 15).
-- Projectile archetype (long forward box). Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "Piercing"
skill.totalDurationMs  = 600
skill.interruptible    = true

skill:addVFX(15, "effects/piercing.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(100, "PlayVFX", {
    vfxId  = 15,
    offset = Vec3(0.0, 1.0, 0.8)
})

local onHit = OnHit({
    damage          = 36,
    vfxId           = 255,
    impulseStrength = 600.0,
    impulseDir      = Vec3(0.0, 0.1, 1.0)
})

skill:addEvent(130, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, -0.2, 2.8, 0.5, 0.5, 2.8, 0, 0, 0) },
    attach              = BoneAttach("spine_02"),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 600,
    onHit               = onHit
})

skill:addEvent(420, "DestroyHitbox", { slot = 0 })

return skill
