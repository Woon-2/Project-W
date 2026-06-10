-- crystals_cross_fade.lua
-- Foundation skill paired 1:1 with the "Crystals Cross Fade" VFX (vfxId 9).
-- Ground-AoE archetype (surround). Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "CrystalsCrossFade"
skill.totalDurationMs  = 1400
skill.cooldownMs       = 1900
skill.interruptible    = true

skill:addVFX(9, "effects/crystals_cross_fade.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(150, "PlayVFX", {
    vfxId  = 9,
    offset = Vec3(0.0, 0.0, 0.0),
    groundLock = true,
    groundSnap = true,
    particleConform = "SnapAndAlign"
})

local onHit = OnHit({
    damage          = 48,
    vfxId           = 255,
    impulseStrength = 600.0,
    impulseDir      = Vec3(0.0, 0.7, 0.5)
})

skill:addEvent(160, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, 0.0, 0.0, 0.4, 1.2, 0.4, 0, 0, 0) },
    attach              = VFXParticleAttach(9, 1),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 1300,
    useParticleSize     = false,
    onHit               = onHit
})

skill:addEvent(1300, "DestroyHitbox", { slot = 0 })

return skill