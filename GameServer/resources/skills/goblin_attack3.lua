-- goblin_attack3.lua
-- Goblin melee attack 3. Clip "Goblin_Attack3" via attackIndex (attackClips_[2]).
-- Minimal foundation; tune in the in-game skill editor.

local skill = Skill()
skill.name            = "Goblin_Attack3"
skill.totalDurationMs = 1000
skill.interruptible   = true

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Goblin_Attack3",
    attackIndex = 2,
    blendTime   = 0.1
})

skill:addEvent(400, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, 0.0, 1.2, 0.8, 0.7, 0.8, 0, 0, 0) },
    attach              = BoneAttach("spine_01"),   -- placeholder: tune per skeleton in editor
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 0,
    onHit               = OnHit({ damage = 15, impulseStrength = 900.0, impulseDir = Vec3(0, 0, 1) })
})

skill:addEvent(640, "DestroyHitbox", { slot = 0 })

return skill
