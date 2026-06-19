-- bomber_attack1.lua
-- Bomber attack 1. Clip "Bomber_Attack1" via attackIndex (attackClips_[0]).
-- Minimal foundation; tune in the in-game skill editor.

local skill = Skill()
skill.name            = "Bomber_Attack1"
skill.totalDurationMs = 900
skill.interruptible   = true

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Bomber_Attack1",
    attackIndex = 0,
    blendTime   = 0.1
})

skill:addEvent(400, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, 0.0, 1.0, 0.8, 0.8, 0.8, 0, 0, 0) },
    attach              = BoneAttach("spine_01"),   -- placeholder: tune per skeleton in editor
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 0,
    onHit               = OnHit({ damage = 18, impulseStrength = 1000.0, impulseDir = Vec3(0, 0, 1) })
})

skill:addEvent(620, "DestroyHitbox", { slot = 0 })

return skill
