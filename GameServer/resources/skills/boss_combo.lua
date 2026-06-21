-- boss_combo.lua
-- Boss melee attack 1: a two-stage combo (two separate hit windows).
-- Clip "Boss_Combo" is selected via attackIndex (AnimBlenderBoss.attackClips_[1]).
-- Root-relative hitbox; tune box/timing in the editor.

local skill = Skill()
skill.name            = "Boss_Combo"
skill.totalDurationMs = 3200
skill.interruptible   = true

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Boss_Combo",
    attackIndex = 1,
    blendTime   = 0.1
})

-- First strike.
skill:addEvent(700, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, 0.6, 1.8, 1.6, 1.1, 1.4, 0, 0, 0) },
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 500,
    onHit               = OnHit({ damage = 22, vfxId = 255, impulseStrength = 700.0, impulseDir = Vec3(0, 0.1, 1) })
})
skill:addEvent(1000, "DestroyHitbox", { slot = 0 })

-- Second, stronger strike.
skill:addEvent(1700, "SpawnHitbox", {
    slot                = 1,
    localOBBs           = { OBB(0.0, 0.6, 2.0, 2.0, 1.2, 1.6, 0, 0, 0) },
    applyAttachRotation = true,
    hitGroup            = 1,
    hitGroupCooldownMs  = 500,
    onHit               = OnHit({ damage = 30, vfxId = 255, impulseStrength = 1200.0, impulseDir = Vec3(0, 0.25, 1) })
})
skill:addEvent(2100, "DestroyHitbox", { slot = 1 })

return skill
