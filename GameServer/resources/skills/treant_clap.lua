-- treant_clap.lua
-- Treant double-hand clap. Clip "Treant_Clap" via attackIndex (attackClips_[1]).
-- Minimal foundation; tune in the in-game skill editor.

local skill = Skill()
skill.name            = "Treant_Clap"
skill.totalDurationMs = 1000
skill.interruptible   = true

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Treant_Clap",
    attackIndex = 1,
    blendTime   = 0.1
})

skill:addEvent(420, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, 0.0, 1.2, 0.9, 1.0, 0.9, 0, 0, 0) },
    attach              = BoneAttach("spine_01"),   -- placeholder: tune per skeleton in editor
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 0,
    onHit               = OnHit({ damage = 22, impulseStrength = 1000.0, impulseDir = Vec3(0, 0, 1) })
})

skill:addEvent(640, "DestroyHitbox", { slot = 0 })

return skill
