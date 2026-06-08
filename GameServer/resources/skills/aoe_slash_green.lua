-- aoe_slash_green.lua
-- Foundation skill paired 1:1 with the "AoE Slash Green" VFX (vfxId 7).
-- Melee-arc archetype (wide). Tune via the in-game skill editor, then port values back here.

local skill = Skill()
skill.name             = "AoESlashGreen"
skill.totalDurationMs  = 600
skill.interruptible    = true

skill:addVFX(7, "effects/aoe_slash_green.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(100, "PlayVFX", {
    vfxId  = 7,
    offset = Vec3(0.0, 1.0, 1.6)
})

local onHit = OnHit({
    damage          = 40,
    vfxId           = 255,
    impulseStrength = 600.0,
    impulseDir      = Vec3(0.0, 0.2, 1.0)
})

skill:addEvent(120, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, -0.6, 1.5, 1.8, 0.8, 1.2, 0, 0, 0) },
    attach              = BoneAttach("spine_02"),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 600,
    onHit               = onHit
})

skill:addEvent(400, "DestroyHitbox", { slot = 0 })

return skill
