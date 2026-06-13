-- arrow_rain.lua
-- Foundation skill paired 1:1 with the "Arrow Rain" VFX (vfxId 12).
-- Ground-AoE archetype (drop zone). Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "ArrowRain"
skill.totalDurationMs  = 2000
skill.interruptible    = true

skill:addVFX(12, "effects/arrow_rain.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(120, "PlayVFX", {
    vfxId  = 12,
    offset = Vec3(0.0, -1.5, 10.0),
    groundSnap = true,
    particleCollision = "GroundKill"
})

local onHit = OnHit({
    damage          = 4,
    vfxId           = 255,
    impulseStrength = 300.0,
    impulseDir      = Vec3(0.0, -0.88, 0.47)
})

skill:addEvent(130, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, 0.0, -0.3, 0.8, 0.6, 0.8, 0, 0, 0) },
    attach              = VFXParticleAttach(12, 0),
    applyAttachRotation = true,
    useParticleSize     = false,
    hitGroup            = 0,
    hitGroupCooldownMs  = 200,
    onHit               = onHit
})

skill:addEvent(2000, "DestroyHitbox", { slot = 0 })

return skill
