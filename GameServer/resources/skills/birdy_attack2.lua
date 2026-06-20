-- birdy_attack2.lua
-- Birdy dive attack 2. Clip "Birdy_Attack2" via attackIndex (attackClips_[1]).
-- Minimal foundation; tune in the in-game skill editor.

local skill = Skill()
skill.name            = "Birdy_Attack2"
skill.totalDurationMs = 800
skill.interruptible   = true

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Birdy_Attack2",
    attackIndex = 1,
    blendTime   = 0.1
})

skill:addEvent(250, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, 0.0, 0.2, 0.5, 0.5, 0.5, 0, 0, 0) },
    attach              = BoneAttach("Claw.L"),   -- placeholder: tune per skeleton in editor
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 700,
    onHit               = OnHit({ damage = 11, impulseStrength = 650.0, impulseDir = Vec3(0, 0, 1) })
})

skill:addEvent(650, "DestroyHitbox", { slot = 0 })

return skill
