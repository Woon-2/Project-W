-- slash_7.lua
-- Foundation skill paired 1:1 with the "Slash 7" VFX (vfxId 4).
-- Melee-arc archetype. Tune via the in-game skill editor, then port values back here.

local skill = Skill()
skill.name             = "Slash7"
skill.totalDurationMs  = 500
skill.interruptible    = true

skill:addVFX(4, "effects/slash_7.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(100, "PlayVFX", {
    vfxId  = 4,
    offset = Vec3(0.0, 1.0, 1.4)
})

local onHit = OnHit({
    damage          = 30,
    vfxId           = 255,
    impulseStrength = 700.0,
    impulseDir      = Vec3(0.0, 0.1, 1.0)
})

skill:addEvent(120, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, -0.6, 1.2, 1.3, 0.8, 0.9, 0, 0, 0) },
    attach              = BoneAttach("spine_02"),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 600,
    onHit               = onHit
})

skill:addEvent(320, "DestroyHitbox", { slot = 0 })

return skill
