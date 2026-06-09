-- arrow_volley.lua
-- Foundation skill paired 1:1 with the "Arrow Volley" VFX (vfxId 11).
-- Projectile archetype (wide forward fan). Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "ArrowVolley"
skill.totalDurationMs  = 700
skill.interruptible    = true

skill:addVFX(11, "effects/arrow_volley.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(120, "PlayVFX", {
    vfxId  = 11,
    offset = Vec3(0.0, 1.0, 0.8)
})

local onHit = OnHit({
    damage          = 28,
    vfxId           = 255,
    impulseStrength = 400.0,
    impulseDir      = Vec3(0.0, 0.1, 1.0)
})

local count = 10

for i = 0, count - 1 do
    skill:addEvent(130, "SpawnHitbox", {
        slot                = i,
        localOBBs           = { OBB(0.0, 0.0, -0.3, 0.25, 0.25, 0.6, 0, 0, 0) },
        attach              = VFXParticleAttach(11, i),
        applyAttachRotation = true,
        useParticleSize     = false,
        hitGroup            = i,
        hitGroupCooldownMs  = 550,
        onHit               = onHit
    })
    skill:addEvent(650, "DestroyHitbox", { slot = i })
end


return skill
