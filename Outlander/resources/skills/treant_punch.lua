-- treant_punch.lua
-- Treant forward punch. Clip "Treant_Punch" via attackIndex (attackClips_[2]).
-- Minimal foundation; tune in the in-game skill editor.

local skill = Skill()
skill.name            = "Treant_Punch"
skill.totalDurationMs = 1500
skill.interruptible   = true

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Treant_Punch",
    attackIndex = 2,
    blendTime   = 0.1
})

skill:addEvent(950, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(-0.3, 0.0, 0.0, 0.6, 0.4, 0.65, 0, 0, 0) },
    attach              = BoneAttach("TreantRPalm"),   -- placeholder: tune per skeleton in editor
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 500,
    onHit               = OnHit({ damage = 22, impulseStrength = 1000.0, impulseDir = Vec3(0, 0, 1) })
})

skill:addEvent(1200, "DestroyHitbox", { slot = 0 })

return skill
