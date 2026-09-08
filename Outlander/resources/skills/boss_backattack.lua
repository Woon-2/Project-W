-- boss_backattack.lua
-- Boss melee attack 2: a sweeping attack that also strikes behind the boss.
-- Clip "Boss_BackAttack" is selected via attackIndex (AnimBlenderBoss.attackClips_[2]).
-- Root-relative hitbox spanning front+back (cz=0, large depth); tune in the editor.

local skill = Skill()
skill.name            = "Boss_BackAttack"
skill.totalDurationMs = 2600
skill.interruptible   = true

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Boss_BackAttack",
    attackIndex = 2,
    blendTime   = 0.1
})

skill:addEvent(850, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.25, 0.1, 0.25, 0.9, 1.2, 2.5, 0, -1, 23) },
    attach              = BoneAttach("weapon_r"),   -- placeholder: tune per skeleton in editor,
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 800,
    onHit               = OnHit({ damage = 32, vfxId = 255, impulseStrength = 900.0, impulseDir = Vec3(0, 0.01, 0) })
})

skill:addEvent(1300, "DestroyHitbox", { slot = 0 })

return skill
