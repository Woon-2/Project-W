-- piercing_circle_slash.lua
-- Foundation skill paired 1:1 with the "Piercing Circle Slash" VFX (vfxId 17).
-- Melee-arc archetype (surround). Tune via the in-game skill editor, then port values back here.

local skill = Skill()
skill.name             = "PiercingCircleSlash"
skill.totalDurationMs  = 600
skill.interruptible    = true

skill:addVFX(17, "effects/piercing_circle_slash.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(100, "PlayVFX", {
    vfxId  = 17,
    offset = Vec3(0.0, 1.0, 0.0)
})

local onHit = OnHit({
    damage          = 38,
    vfxId           = 255,
    impulseStrength = 500.0,
    impulseDir      = Vec3(0.0, 0.3, 1.0)
})

skill:addEvent(120, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, -0.6, 0.0, 1.8, 0.8, 1.8, 0, 0, 0) },
    attach              = BoneAttach("spine_02"),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 600,
    onHit               = onHit
})

skill:addEvent(400, "DestroyHitbox", { slot = 0 })

return skill
