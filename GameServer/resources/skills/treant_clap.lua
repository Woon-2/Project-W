-- treant_clap.lua
-- Treant double-hand clap. Clip "Treant_Clap" via attackIndex (attackClips_[1]).
-- Minimal foundation; tune in the in-game skill editor.

local skill = Skill()
skill.name            = "Treant_Clap"
skill.totalDurationMs = 600
skill.interruptible   = true

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Treant_Clap",
    attackIndex = 1,
    blendTime   = 0.1
})

skill:addEvent(300, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(-0.3, 0.0, 0.0, 0.55, 0.4, 0.45, 0, 0, 0) },
    attach              = BoneAttach("TreantLPalm"),   -- placeholder: tune per skeleton in editor
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 350,
    onHit               = OnHit({ damage = 22, impulseStrength = 1000.0, impulseDir = Vec3(0, 0, 1) })
})

skill:addEvent(300, "SpawnHitbox", {
    slot                = 1,
    localOBBs           = { OBB(-0.3, 0.0, 0.0, 0.55, 0.4, 0.45, 0, 0, 0) },
    attach              = BoneAttach("TreantRPalm"),   -- placeholder: tune per skeleton in editor
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 350,
    onHit               = OnHit({ damage = 22, impulseStrength = 1000.0, impulseDir = Vec3(0, 0, 1) })
})

skill:addEvent(500, "DestroyHitbox", { slot = 0 })
skill:addEvent(500, "DestroyHitbox", { slot = 1 })

return skill
