-- red_energy_explosion.lua
-- Foundation skill paired 1:1 with the "Red Energy Explosion" VFX (vfxId 8).
-- Ground-AoE archetype (burst around caster). Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "RedEnergyExplosion"
skill.totalDurationMs  = 800
skill.interruptible    = true

skill:addVFX(8, "effects/red_energy_explosion.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(150, "PlayVFX", {
    vfxId  = 8,
    offset = Vec3(0.0, 0.8, 0.0)
})

local onHit = OnHit({
    damage          = 50,
    vfxId           = 255,
    impulseStrength = 900.0,
    impulseDir      = Vec3(0.0, 0.5, 1.0)
})

skill:addEvent(250, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, -0.4, 0.0, 2.2, 1.4, 2.2, 0, 0, 0) },
    attach              = BoneAttach("spine_02"),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 600,
    onHit               = onHit
})

skill:addEvent(550, "DestroyHitbox", { slot = 0 })

return skill
