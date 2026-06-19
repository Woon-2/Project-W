-- treant_spinkick.lua
-- Treant spin kick. Clip "Treant_SpinKick" via attackIndex (attackClips_[0]).
-- Minimal foundation; tune in the in-game skill editor.

local skill = Skill()
skill.name            = "Treant_SpinKick"
skill.totalDurationMs = 1100
skill.interruptible   = true

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Treant_SpinKick",
    attackIndex = 0,
    blendTime   = 0.1
})

-- Wide sweep box for a spinning kick.
skill:addEvent(450, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, 0.0, 1.0, 1.4, 0.7, 1.4, 0, 0, 0) },
    attach              = BoneAttach("spine_01"),   -- placeholder: tune per skeleton in editor
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 0,
    onHit               = OnHit({ damage = 20, impulseStrength = 1100.0, impulseDir = Vec3(0, 0, 1) })
})

skill:addEvent(700, "DestroyHitbox", { slot = 0 })

return skill
