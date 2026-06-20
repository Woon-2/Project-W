-- boss_attack.lua
-- Temporary boss attack. Clip "Boss_Attack" via attackIndex (AnimBlenderBoss).
-- Uses a root-relative hitbox until the boss skeleton attach bone is chosen.

local skill = Skill()
skill.name            = "Boss_Attack"
skill.totalDurationMs = 4200
skill.interruptible   = true

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Boss_Attack",
    attackIndex = 0,
    blendTime   = 0.1
})

skill:addEvent(1650, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, 0.4, 1.9, 1.6, 1.2, 1.8, 0, 0, 0) },
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 800,
    onHit               = OnHit({ damage = 30, impulseStrength = 1200.0, impulseDir = Vec3(0, 0, 1) })
})

skill:addEvent(2300, "DestroyHitbox", { slot = 0 })

return skill
