-- mushroom_attack1.lua
-- Mushroom melee attack 1. Clip "Mushroom_Attack1" via attackIndex (attackClips_[0]).
-- Minimal foundation; tune in the in-game skill editor.

local skill = Skill()
skill.name            = "Mushroom_Attack1"
skill.totalDurationMs = 1300
skill.interruptible   = true

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Mushroom_Attack1",
    attackIndex = 0,
    blendTime   = 0.1
})

skill:addEvent(700, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, 0.0, 0.2, 0.45, 0.7, 0.8, 0, 0, 0) },
    attach              = BoneAttach("Bone003"),   -- placeholder: tune per skeleton in editor
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 700,
    onHit               = OnHit({ damage = 10, impulseStrength = 600.0, impulseDir = Vec3(0, 0, 1) })
})

skill:addEvent(1100, "DestroyHitbox", { slot = 0 })

return skill
