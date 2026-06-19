-- slime_attack1.lua
-- Slime body-slam attack. Clip "Slime_Attack1" via attackIndex (attackClips_[0]).
-- Minimal foundation; tune in the in-game skill editor.

local skill = Skill()
skill.name            = "Slime_Attack1"
skill.totalDurationMs = 750
skill.interruptible   = true

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Slime_Attack1",
    attackIndex = 0,
    blendTime   = 0.1
})

skill:addEvent(300, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, 0.0, 0.8, 0.7, 0.5, 0.7, 0, 0, 0) },
    attach              = BoneAttach("spine_01"),   -- placeholder: tune per skeleton in editor
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 0,
    onHit               = OnHit({ damage = 9, impulseStrength = 550.0, impulseDir = Vec3(0, 0, 1) })
})

skill:addEvent(490, "DestroyHitbox", { slot = 0 })

return skill
