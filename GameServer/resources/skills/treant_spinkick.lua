-- treant_spinkick.lua
-- Treant spin kick. Clip "Treant_SpinKick" via attackIndex (attackClips_[0]).
-- Minimal foundation; tune in the in-game skill editor.

local skill = Skill()
skill.name            = "Treant_SpinKick"
skill.totalDurationMs = 1600
skill.interruptible   = true

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Treant_SpinKick",
    attackIndex = 0,
    blendTime   = 0.1
})

-- Wide sweep box for a spinning kick.
skill:addEvent(600, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, 0.0, 0.0, 0.5, 0.7, 0.5, 0, 0, 0) },
    attach              = BoneAttach("TreantLThigh"),   -- placeholder: tune per skeleton in editor
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 1200,
    onHit               = OnHit({ damage = 20, impulseStrength = 1100.0, impulseDir = Vec3(0, 0, 1) })
})

skill:addEvent(600, "SpawnHitbox", {
    slot                = 1,
    localOBBs           = { OBB(0.0, 0.0, 0.0, 0.5, 0.7, 0.5, 0, 0, 0) },
    attach              = BoneAttach("TreantLCalf"),   -- placeholder: tune per skeleton in editor
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 1200,
    onHit               = OnHit({ damage = 20, impulseStrength = 1100.0, impulseDir = Vec3(0, 0, 1) })
})

skill:addEvent(600, "SpawnHitbox", {
    slot                = 2,
    localOBBs           = { OBB(0.0, -0.1, 0.0, 0.5, 0.9, 0.5, 0, 0, 0) },
    attach              = BoneAttach("TreantLFoot"),   -- placeholder: tune per skeleton in editor
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 1200,
    onHit               = OnHit({ damage = 20, impulseStrength = 1100.0, impulseDir = Vec3(0, 0, 1) })
})

skill:addEvent(1400, "DestroyHitbox", { slot = 0 })
skill:addEvent(1400, "DestroyHitbox", { slot = 1 })
skill:addEvent(1400, "DestroyHitbox", { slot = 2 })

return skill
