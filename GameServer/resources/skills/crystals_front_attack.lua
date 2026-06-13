-- crystals_front_attack.lua
-- Foundation skill paired 1:1 with the "Crystals Front Attack" VFX (vfxId 6).
-- Ground-AoE archetype (front line). Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "CrystalsFrontAttack"
skill.totalDurationMs  = 800
skill.interruptible    = true

skill:addVFX(6, "effects/crystals_front_attack.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(150, "PlayVFX", {
    vfxId  = 6,
    offset = Vec3(0.0, 0.0, 0.5),
    orient = {-90, 0, 0},
    groundLock = true,
    groundSnap = true,
    particleConform = "SnapAndAlign"
})

local onHit = OnHit({
    damage          = 42,
    vfxId           = 255,
    impulseStrength = 500.0,
    impulseDir      = Vec3(0.0, 0.7, 0.5)
})

skill:addEvent(120, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, 0.0, 0.0, 0.8, 2.4, 0.8, 0, 0, 0) },
    attach              = VFXParticleAttach(6, 0),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 600,
    useParticleSize     = true,
    onHit               = onHit
})

skill:addEvent(600, "DestroyHitbox", { slot = 0 })

return skill