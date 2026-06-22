-- mushroom_attack2.lua
-- Mushroom melee attack 2. Clip "Mushroom_Attack2" via attackIndex (attackClips_[1]).
-- Minimal foundation; tune in the in-game skill editor.

local skill = Skill()
skill.name            = "Mushroom_Attack2"
skill.totalDurationMs = 1400
skill.interruptible   = true

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Mushroom_Attack2",
    attackIndex = 1,
    blendTime   = 0.1
})

skill:addEvent(800, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, 0.0, 0.2, 0.45, 0.7, 0.8, 0, 0, 0) },
    attach              = BoneAttach("Bone003"),   -- placeholder: tune per skeleton in editor
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 700,
    onHit               = OnHit({ damage = 13, impulseStrength = 750.0, impulseDir = Vec3(0, 0, 1) })
})

skill:addEvent(1200, "DestroyHitbox", { slot = 0 })

return skill
