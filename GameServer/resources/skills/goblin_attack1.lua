-- goblin_attack1.lua
-- Goblin melee attack 1. Minimal foundation (PlayAnimation + one hitbox);
-- tune box/timing/bone in the in-game skill editor.
-- Clip "Goblin_Attack1" is selected via attackIndex (AnimBlenderGoblin.attackClips_[0]).

local skill = Skill()
skill.name            = "Goblin_Attack1"
skill.totalDurationMs = 1450
skill.interruptible   = true

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Goblin_Attack1",
    attackIndex = 0,
    blendTime   = 0.1
})

skill:addEvent(950, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.05, -0.5, -0.45, 0.15, 0.15, 0.6, 0, -39, 0) },
    attach              = BoneAttach("R_w"),   -- placeholder: tune per skeleton in editor
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 700,
    onHit               = OnHit({ damage = 10, vfxId = 255, impulseStrength = 600.0, impulseDir = Vec3(0, 0, 1) })
})

skill:addEvent(1200, "DestroyHitbox", { slot = 0 })

return skill