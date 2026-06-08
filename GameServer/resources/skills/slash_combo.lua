-- slash_combo.lua
-- Foundation skill paired 1:1 with the "Slash Combo" VFX (vfxId 3).
-- Melee-arc archetype. Tune via the in-game skill editor, then port values back here.

local skill = Skill()
skill.name             = "SlashCombo"
skill.totalDurationMs  = 600
skill.interruptible    = true

skill:addVFX(3, "effects/slash_combo.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(100, "PlayVFX", {
    vfxId  = 3,
    offset = Vec3(0.0, 1.0, 1.4)
})

local onHit = OnHit({
    damage          = 35,
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

skill:addEvent(400, "DestroyHitbox", { slot = 0 })

return skill
