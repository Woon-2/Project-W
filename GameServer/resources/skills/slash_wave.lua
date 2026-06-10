-- slash_wave.lua
-- Foundation skill paired 1:1 with the "Slash Wave" VFX (vfxId 2).
-- Melee-arc archetype. Tune via the in-game skill editor, then port values back here.

local skill = Skill()
skill.name             = "SlashWave"
skill.totalDurationMs  = 1000
skill.cooldownMs       = 1500
skill.interruptible    = true

skill:addVFX(2, "effects/slash_wave.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(100, "PlayVFX", {
    vfxId  = 2,
    offset = Vec3(0.0, 1.0, 1.4)
})

local onHit = OnHit({
    damage          = 20,
    vfxId           = 0,
    impulseStrength = 700.0,
    impulseDir      = Vec3(0.0, 0.0, 1.0)
})

skill:addEvent(120, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, -1.3, -0.85, 0.3, 1.2, 0.75, 0, 45, 0) },
    attach              = BoneAttach("spine_01"),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 600,
    onHit               = onHit
})

skill:addEvent(150, "SpawnHitbox", {
    slot                = 1,
    localOBBs           = { OBB(0.0, -1.45, 0, 0.3, 1.25, 1.15, 0, 0, 0) },
    attach              = BoneAttach("spine_01"),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 600,
    onHit               = onHit
})

skill:addEvent(180, "SpawnHitbox", {
    slot                = 2,
    localOBBs           = { OBB(0.0, -1.3, 0.85, 0.3, 1.2, 0.75, 0, -45, 0) },
    attach              = BoneAttach("spine_01"),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 600,
    onHit               = onHit
})

skill:addEvent(120, "SpawnHitbox", {
    slot                = 3,
    localOBBs           = { OBB(0.0, 1.5, 0.0, 0.8, 2.0, 0.25, 0, 0, 0) },
    attach              = VFXParticleAttach(2, 1),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 600,
    onHit               = onHit
})

skill:addEvent(320, "DestroyHitbox", { slot = 0 })
skill:addEvent(350, "DestroyHitbox", { slot = 1 })
skill:addEvent(380, "DestroyHitbox", { slot = 2 })
skill:addEvent(900, "DestroyHitbox", { slot = 3 })

return skill
