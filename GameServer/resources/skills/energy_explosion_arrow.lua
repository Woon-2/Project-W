-- energy_explosion_arrow.lua
-- Foundation skill paired 1:1 with the "Energy Explosion Arrow" VFX (vfxId 13).
-- Projectile-into-burst archetype. Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "EnergyExplosionArrow"
skill.totalDurationMs  = 800
skill.interruptible    = true

skill:addVFX(13, "effects/energy_explosion_arrow.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(120, "PlayVFX", {
    vfxId  = 13,
    offset = Vec3(0.0, 1.0, 0.8)
})

local onHit = OnHit({
    damage          = 45,
    vfxId           = 255,
    impulseStrength = 800.0,
    impulseDir      = Vec3(0.0, 0.3, 1.0)
})

skill:addEvent(160, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, -0.2, 3.0, 0.5, 0.5, 3.0, 0, 0, 0) },
    attach              = BoneAttach("spine_02"),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 600,
    onHit               = onHit
})

skill:addEvent(600, "DestroyHitbox", { slot = 0 })

return skill
