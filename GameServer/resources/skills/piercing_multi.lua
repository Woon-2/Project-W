-- piercing_multi.lua
-- Foundation skill paired 1:1 with the "Piercing Multi" VFX (vfxId 18).
-- Multi-projectile archetype (wide forward box). Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "PiercingMulti"
skill.totalDurationMs  = 1100
skill.cooldownMs       = 1600
skill.interruptible    = true

skill:addVFX(18, "effects/piercing_multi.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(100, "PlayVFX", {
    vfxId  = 18,
    offset = Vec3(0.0, 1.0, 0.8)
})

local onHit = OnHit({
    damage          = 8,
    vfxId           = 255,
    impulseStrength = 450.0,
    impulseDir      = Vec3(0.0, 0.0, 1.0)
})

local count = 20

for i = 0, count - 1 do
    skill:addEvent(130, "SpawnHitbox", {
        slot                = i,
        localOBBs           = { OBB(2.0, 0.0, 0.0, 3.0, 1.2, 1.2, 0, 0, 0) },
        attach              = VFXParticleAttach(18, i),
        applyAttachRotation = true,
        useParticleSize     = false,
        hitGroup            = 0,
        hitGroupCooldownMs  = 300,
        onHit               = onHit
    })
    skill:addEvent(330 + i * 30, "DestroyHitbox", { slot = i })
end

return skill
