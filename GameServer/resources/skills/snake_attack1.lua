-- snake_attack1.lua
-- Snake bite attack. Clip "Snake_Attack1" via attackIndex (attackClips_[0]).
-- Minimal foundation; tune in the in-game skill editor.

local skill = Skill()
skill.name            = "Snake_Attack1"
skill.totalDurationMs = 700
skill.interruptible   = true

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Snake_Attack1",
    attackIndex = 0,
    blendTime   = 0.1
})

skill:addEvent(250, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, 0.0, 0.9, 0.5, 0.5, 0.6, 0, 0, 0) },
    attach              = BoneAttach("spine_01"),   -- placeholder: tune per skeleton in editor
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 0,
    onHit               = OnHit({ damage = 8, impulseStrength = 500.0, impulseDir = Vec3(0, 0, 1) })
})

skill:addEvent(430, "DestroyHitbox", { slot = 0 })

return skill
