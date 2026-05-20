-- sword_slash.lua
-- Basic melee sword slash skill.
-- Timeline (ms):
--   0   : trigger attack animation
--   100 : spawn hitbox on Weapon_R bone + play sword trail VFX
--   400 : destroy hitbox
--   600 : camera shake

local skill = Skill()
skill.name             = "SwordSlash"
skill.totalDurationMs  = 800
skill.interruptible    = true

-- VFX registry (indexed 0-based; SkillCompiler converts to asset.vfxNames[])
skill:addVFX(0, "effects/blood_hit.json")
skill:addVFX(1, "effects/sword_slash_1.json")

-- t=0ms: trigger attack animation
skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

-- t=100ms: spawn hitbox attached to the Weapon_R bone
skill:addEvent(100, "SpawnHitbox", {
    slot      = 0,
    localOBBs = {
        OBB(0.0, 0.0, 0.4,   -- center (forward offset along bone)
            0.3, 0.4, 0.4)   -- halfExtents (roughly 0.6x0.8x0.8 AABB equivalent)
    },
    attach = BoneAttach("Weapon_R"),
    onHit  = OnHit({
        damage          = 5,
        vfxId           = 0,    -- blood_hit.json
        impulseStrength = 2400.0,
        impulseDir      = Vec3(0.0, 0.3, 1.0)
    })
})

-- t=100ms: play sword trail VFX at Weapon_R bone
skill:addEvent(100, "PlayVFX", {
    vfxId  = 1,
    attach = BoneAttach("Weapon_R")
})

-- t=400ms: destroy hitbox (slot 0)
skill:addEvent(400, "DestroyHitbox", { slot = 0 })

-- t=600ms: small camera shake for impact feel
skill:addEvent(600, "CameraShake", {
    magnitude  = 0.3,
    durationMs = 150
})

return skill
