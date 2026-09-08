-- goblin_attack2.lua
-- Goblin melee attack 2. Clip "Goblin_Attack2" via attackIndex (attackClips_[1]).
-- Minimal foundation; tune in the in-game skill editor.

local skill = Skill()
skill.name            = "Goblin_Attack2"
skill.totalDurationMs = 1600
skill.interruptible   = true

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Goblin_Attack2",
    attackIndex = 1,
    blendTime   = 0.1
})

skill:addEvent(1150, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.05, -0.5, -0.45, 0.3, 0.3, 0.75, 0, -39, 0) },
    attach              = BoneAttach("R_w"),   -- placeholder: tune per skeleton in editor
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 700,
    onHit               = OnHit({ damage = 12, vfxId = 255, impulseStrength = 700.0, impulseDir = Vec3(0, 0, 1) })
})

skill:addEvent(1500, "DestroyHitbox", { slot = 0 })

return skill
