-- arrow_volley.lua
-- Foundation skill paired 1:1 with the "Arrow Volley" VFX (vfxId 11).
-- Projectile archetype (wide forward fan). Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "ArrowVolley"
skill.weapon           = "bow"
skill.dialSlot         = 0
skill.chargeCost       = 3
skill.cooldownMs       = 1100
skill.totalDurationMs  = 700
skill.interruptible    = true

-- systems[1..9] = arrow meshes fanned by yaw (code-built; no JSON source).
-- Mirrors client game.cpp: spread 56 deg, 9 arrows, lifetime 0.45, speed 38.
-- System index 9 (10th) is the shared ArrowHit sub-emitter child (no entry).
local volleyCount  = 9
local volleySpread = 56.0
local volleySystems = {}
for i = 1, volleyCount do
    local yaw = -volleySpread * 0.5 + volleySpread / (volleyCount - 1) * (i - 1)
    volleySystems[i] = {
        mode = "Emit", looping = false,
        lifetime = 0.45, speed = 38.0, startSize = 0.3,
        shapeType = "Point", direction = Vec3(0.0, 0.0, 1.0),
        shapeEuler = Vec3(0.0, yaw, 0.0),
        meshEuler  = Vec3(0.0, yaw, 0.0),
        maxParticles = 32,
    }
end
skill:addVFX(11, "effects/arrow_volley.json", { systems = volleySystems })

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Combat_Bow_Attack",
    attackIndex = 0,
    blendTime   = 0.1
})

skill:addEvent(120, "PlayVFX", {
    vfxId  = 11,
    offset = Vec3(0.0, 1.0, 0.8)
})

-- Bow release SFX, on the volley shot.
skill:addEvent(120, "PlaySound", { sound = "arrow_default" })

local onHit = OnHit({
    damage          = 28,
    vfxId           = 255,
    impulseStrength = 400.0,
    impulseDir      = Vec3(0.0, 0.1, 1.0)
})

-- NOTE: was 10, but the effect has 9 arrow systems (kArrowVolleyCount); the
-- 10th slot attached the shared ArrowHit child system by accident.
local count = 9

for i = 0, count - 1 do
    skill:addEvent(130, "SpawnHitbox", {
        slot                = i,
        localOBBs           = { OBB(0.0, 0.0, -0.3, 0.25, 0.25, 0.6, 0, 0, 0) },
        attach              = VFXParticleAttach(11, i),
        applyAttachRotation = true,
        useParticleSize     = false,
        penetrate           = false,   -- non-penetrating: each arrow particle dies on its first hit
        hitGroup            = i,
        hitGroupCooldownMs  = 550,
        onHit               = onHit
    })
    skill:addEvent(650, "DestroyHitbox", { slot = i })
end


return skill
