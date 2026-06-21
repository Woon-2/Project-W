-- boss_smite.lua
-- Boss melee attack 3: a heavy overhead ground slam.
-- Clip "Boss_Smite" is selected via attackIndex (AnimBlenderBoss.attackClips_[3]).
-- Root-relative hitbox in front, larger window; tune box/timing in the editor.

local skill = Skill()
skill.name            = "Boss_Smite"
skill.totalDurationMs = 3600
skill.interruptible   = false

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Boss_Smite",
    attackIndex = 3,
    blendTime   = 0.1
})

skill:addEvent(1500, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, 0.3, 2.2, 2.4, 1.4, 2.0, 0, 0, 0) },
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 1000,
    onHit               = OnHit({ damage = 55, vfxId = 255, impulseStrength = 1800.0, impulseDir = Vec3(0, 0.4, 1) })
})

skill:addEvent(2000, "DestroyHitbox", { slot = 0 })

return skill
