-- arrow_rain.lua
-- Foundation skill paired 1:1 with the "Arrow Rain" VFX (vfxId 12).
-- Ground-AoE archetype (drop zone). Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "ArrowRain"
skill.totalDurationMs  = 900
skill.interruptible    = true

skill:addVFX(12, "effects/arrow_rain.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(120, "PlayVFX", {
    vfxId  = 12,
    offset = Vec3(0.0, 3.0, 4.0)
})

local onHit = OnHit({
    damage          = 30,
    vfxId           = 255,
    impulseStrength = 300.0,
    impulseDir      = Vec3(0.0, -0.3, 0.5)
})

skill:addEvent(300, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, -0.8, 4.0, 2.0, 1.5, 2.0, 0, 0, 0) },
    attach              = BoneAttach("spine_02"),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 600,
    onHit               = onHit
})

skill:addEvent(700, "DestroyHitbox", { slot = 0 })

return skill
