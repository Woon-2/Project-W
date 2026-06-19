-- mushroom_attack2.lua
-- Mushroom melee attack 2. Clip "Mushroom_Attack2" via attackIndex (attackClips_[1]).
-- Minimal foundation; tune in the in-game skill editor.

local skill = Skill()
skill.name            = "Mushroom_Attack2"
skill.totalDurationMs = 950
skill.interruptible   = true

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Mushroom_Attack2",
    attackIndex = 1,
    blendTime   = 0.1
})

skill:addEvent(380, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, 0.0, 1.1, 0.7, 0.7, 0.7, 0, 0, 0) },
    attach              = BoneAttach("spine_01"),   -- placeholder: tune per skeleton in editor
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 0,
    onHit               = OnHit({ damage = 13, impulseStrength = 750.0, impulseDir = Vec3(0, 0, 1) })
})

skill:addEvent(600, "DestroyHitbox", { slot = 0 })

return skill
