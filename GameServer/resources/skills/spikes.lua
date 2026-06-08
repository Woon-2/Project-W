-- spikes.lua
-- Foundation skill paired 1:1 with the "Spikes" VFX (vfxId 5).
-- Ground-AoE archetype (front eruption). Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "Spikes"
skill.totalDurationMs  = 800
skill.interruptible    = true

skill:addVFX(5, "effects/spikes.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(150, "PlayVFX", {
    vfxId  = 5,
    offset = Vec3(0.0, 0.0, 2.5)
})

local onHit = OnHit({
    damage          = 40,
    vfxId           = 255,
    impulseStrength = 500.0,
    impulseDir      = Vec3(0.0, 0.8, 0.4)
})

skill:addEvent(250, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, -0.8, 2.5, 1.6, 1.2, 1.6, 0, 0, 0) },
    attach              = BoneAttach("spine_02"),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 600,
    onHit               = onHit
})

skill:addEvent(600, "DestroyHitbox", { slot = 0 })

return skill
