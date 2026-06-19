-- goblin_attack3.lua
-- Goblin melee attack 3. Clip "Goblin_Attack3" via attackIndex (attackClips_[2]).
-- Minimal foundation; tune in the in-game skill editor.

local skill = Skill()
skill.name            = "Goblin_Attack3"
skill.totalDurationMs = 2400
skill.interruptible   = true

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Goblin_Attack3",
    attackIndex = 2,
    blendTime   = 0.1
})

skill:addEvent(1000, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.05, -0.5, -0.45, 0.15, 0.15, 0.6, 0, -39, 0) },
    attach              = BoneAttach("R_w"),   -- placeholder: tune per skeleton in editor
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 700,
    onHit               = OnHit({ damage = 15, vfxId = 255, impulseStrength = 900.0, impulseDir = Vec3(0, 0, 1) })
})

skill:addEvent(1500, "DestroyHitbox", { slot = 0 })

skill:addEvent(1700, "SpawnHitbox", {
    slot                = 1,
    localOBBs           = { OBB(0.05, -0.5, -0.45, 0.15, 0.15, 0.6, 0, -39, 0) },
    attach              = BoneAttach("R_w"),   -- placeholder: tune per skeleton in editor
    applyAttachRotation = true,
    hitGroup            = 1,
    hitGroupCooldownMs  = 700,
    onHit               = OnHit({ damage = 15, vfxId = 255, impulseStrength = 900.0, impulseDir = Vec3(0, 0, 1) })
})

skill:addEvent(2200, "DestroyHitbox", { slot = 1 })

return skill
