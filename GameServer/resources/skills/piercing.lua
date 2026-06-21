-- piercing.lua
-- Foundation skill paired 1:1 with the "Piercing" VFX (vfxId 15).
-- Projectile archetype (long forward box). Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "Piercing"
skill.weapon           = "spear"
skill.isBasic          = true
skill.totalDurationMs  = 600
skill.interruptible    = true

skill:addVFX(0, "effects/blood_hit.json")
skill:addVFX(15, "effects/piercing.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(100, "PlayVFX", {
    vfxId  = 15,
    offset = Vec3(0.0, 1.0, 0.8)
})

-- Spear thrust SFX, on the stab.
skill:addEvent(100, "PlaySound", { sound = "spear1" })

local onHit = OnHit({
    damage          = 16,
    vfxId           = 0,
    impulseStrength = 1200.0,
    impulseDir      = Vec3(0.0, 0.0, 1.0)
})

skill:addEvent(130, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, -2.0, 0.0, 0.5, 3.4, 0.85, 0, 0, 0) },
    attach              = BoneAttach("spine_01"),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 500,
    onHit               = onHit
})

local onHitTip = deepCopy(onHit)
onHitTip.impulseStrength = 500.0

skill:addEvent(300, "SpawnHitbox", {
    slot                = 1,
    localOBBs           = { OBB(0.0, -5.8, 0.0, 0.5, 1.4, 0.7, 0, 0, 0) },
    attach              = BoneAttach("spine_01"),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 500,
    onHit               = onHitTip
})

skill:addEvent(360, "DestroyHitbox", { slot = 0 })
skill:addEvent(400, "DestroyHitbox", { slot = 1 })

return skill
