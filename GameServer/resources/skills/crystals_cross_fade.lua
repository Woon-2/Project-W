-- crystals_cross_fade.lua
-- Foundation skill paired 1:1 with the "Crystals Cross Fade" VFX (vfxId 9).
-- Ground-AoE archetype (surround). Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "CrystalsCrossFade"
skill.totalDurationMs  = 800
skill.interruptible    = true

skill:addVFX(9, "effects/crystals_cross_fade.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(150, "PlayVFX", {
    vfxId  = 9,
    offset = Vec3(0.0, 0.0, 0.0)
})

local onHit = OnHit({
    damage          = 38,
    vfxId           = 255,
    impulseStrength = 450.0,
    impulseDir      = Vec3(0.0, 0.6, 1.0)
})

skill:addEvent(250, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, -0.8, 0.0, 2.0, 1.2, 2.0, 0, 0, 0) },
    attach              = BoneAttach("spine_02"),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 600,
    onHit               = onHit
})

skill:addEvent(600, "DestroyHitbox", { slot = 0 })

return skill
