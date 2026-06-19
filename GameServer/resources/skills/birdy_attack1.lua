-- birdy_attack1.lua
-- Birdy peck attack 1. Clip "Birdy_Attack1" via attackIndex (attackClips_[0]).
-- Minimal foundation; tune in the in-game skill editor.

local skill = Skill()
skill.name            = "Birdy_Attack1"
skill.totalDurationMs = 650
skill.interruptible   = true

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Birdy_Attack1",
    attackIndex = 0,
    blendTime   = 0.1
})

skill:addEvent(220, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, 0.0, 0.8, 0.5, 0.5, 0.5, 0, 0, 0) },
    attach              = BoneAttach("spine_01"),   -- placeholder: tune per skeleton in editor
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 0,
    onHit               = OnHit({ damage = 7, impulseStrength = 450.0, impulseDir = Vec3(0, 0, 1) })
})

skill:addEvent(400, "DestroyHitbox", { slot = 0 })

return skill
