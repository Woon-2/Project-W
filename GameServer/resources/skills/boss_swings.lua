-- boss_swings.lua
-- Boss melee attack 0: a wide horizontal swing.
-- Clip "Boss_Swings" is selected via attackIndex (AnimBlenderBoss.attackClips_[0]).
-- Root-relative hitbox (boss skeleton attach bone TBD); tune box/timing in the editor.

local skill = Skill()
skill.name            = "Boss_Swings"
skill.totalDurationMs = 2400
skill.interruptible   = true

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Boss_Swings",
    attackIndex = 0,
    blendTime   = 0.1
})

skill:addEvent(800, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.25, 0.1, 0.25, 0.9, 1.2, 2.5, 0, -1, 23) },
    attach              = BoneAttach("weapon_r"),   -- placeholder: tune per skeleton in editor,
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 800,
    onHit               = OnHit({ damage = 35, vfxId = 255, impulseStrength = 1100.0, impulseDir = Vec3(0, 0.2, 1) })
})

skill:addEvent(1200, "DestroyHitbox", { slot = 0 })

skill:addEvent(1500, "SpawnHitbox", {
    slot                = 1,
    localOBBs           = { OBB(0.25, 0.1, 0.25, 0.9, 1.2, 2.5, 0, -1, 23) },
    attach              = BoneAttach("weapon_r"),   -- placeholder: tune per skeleton in editor,
    applyAttachRotation = true,
    hitGroup            = 1,
    hitGroupCooldownMs  = 800,
    onHit               = OnHit({ damage = 35, vfxId = 255, impulseStrength = 1100.0, impulseDir = Vec3(0, 0.2, 1) })
})

skill:addEvent(1900, "DestroyHitbox", { slot = 1 })


return skill
