-- treant_punch.lua
-- Treant forward punch. Clip "Treant_Punch" via attackIndex (attackClips_[2]).
-- Minimal foundation; tune in the in-game skill editor.

local skill = Skill()
skill.name            = "Treant_Punch"
skill.totalDurationMs = 900
skill.interruptible   = true

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Treant_Punch",
    attackIndex = 2,
    blendTime   = 0.1
})

skill:addEvent(380, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, 0.0, 1.3, 0.8, 0.8, 1.0, 0, 0, 0) },
    attach              = BoneAttach("spine_01"),   -- placeholder: tune per skeleton in editor
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 0,
    onHit               = OnHit({ damage = 19, impulseStrength = 1050.0, impulseDir = Vec3(0, 0, 1) })
})

skill:addEvent(600, "DestroyHitbox", { slot = 0 })

return skill
