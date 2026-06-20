-- snake_attack1.lua
-- Snake bite attack. Clip "Snake_Attack1" via attackIndex (attackClips_[0]).
-- Minimal foundation; tune in the in-game skill editor.

local skill = Skill()
skill.name            = "Snake_Attack1"
skill.totalDurationMs = 900
skill.interruptible   = true

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Snake_Attack1",
    attackIndex = 0,
    blendTime   = 0.1
})

skill:addEvent(300, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(-0.1, -0.25, 0.05, 0.2, 0.4, 0.15, -2, 13, -18) },
    attach              = BoneAttach("Snake_jnt40"),   -- placeholder: tune per skeleton in editor
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 700,
    onHit               = OnHit({ damage = 8, impulseStrength = 500.0, impulseDir = Vec3(0, 0, 1) })
})

skill:addEvent(800, "DestroyHitbox", { slot = 0 })

return skill
