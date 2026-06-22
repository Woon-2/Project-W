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
skill:addEvent(750, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.2, 0.15, 0.6, 0.9, 1.0, 2.2, 0, -1, 23) },
    attach              = BoneAttach("weapon_r"),   -- placeholder: tune per skeleton in editor,
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 500,
    onHit               = OnHit({ damage = 22, vfxId = 255, impulseStrength = 700.0, impulseDir = Vec3(0, 0.1, 1) })
})
skill:addEvent(1050, "DestroyHitbox", { slot = 0 })

-- Second, stronger strike.
skill:addEvent(1400, "SpawnHitbox", {
    slot                = 1,
    localOBBs           = { OBB(0.2, 0.15, 0.6, 0.9, 1.0, 2.2, 0, -1, 23) },
    attach              = BoneAttach("weapon_r"),   -- placeholder: tune per skeleton in editor,
    applyAttachRotation = true,
    hitGroup            = 1,
    hitGroupCooldownMs  = 500,
    onHit               = OnHit({ damage = 30, vfxId = 255, impulseStrength = 1200.0, impulseDir = Vec3(0, 0.25, 1) })
})
skill:addEvent(1700, "DestroyHitbox", { slot = 1 })

-- Third, strongest strike.
skill:addEvent(2400, "SpawnHitbox", {
    slot                = 2,
    localOBBs           = { OBB(0.2, 0.15, 0.6, 0.9, 1.0, 2.2, 0, -1, 23) },
    attach              = BoneAttach("weapon_r"),   -- placeholder: tune per skeleton in editor,
    applyAttachRotation = true,
    hitGroup            = 2,
    hitGroupCooldownMs  = 500,
    onHit               = OnHit({ damage = 40, vfxId = 255, impulseStrength = 1400.0, impulseDir = Vec3(0, 0.25, 1) })
})
skill:addEvent(2800, "DestroyHitbox", { slot = 2 })

return skill
