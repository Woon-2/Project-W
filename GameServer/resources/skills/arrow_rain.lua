-- arrow_rain.lua
-- Foundation skill paired 1:1 with the "Arrow Rain" VFX (vfxId 12).
-- Ground-AoE archetype (drop zone). Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "ArrowRain"
skill.weapon           = "bow"
skill.dialSlot         = 1
skill.chargeCost       = 5
skill.cooldownMs       = 2400
skill.totalDurationMs  = 2000
skill.interruptible    = true

-- systems[1] = arrow rain parent (code-built; no JSON source). Mirrors client
-- game.cpp: disc (cone r=4.75, angle 0) at y=+11 facing down, 3 arrows x 10
-- cycles every 0.12s, speed 32, lifetime 0.34. The drop XZ scatter is random:
-- the per-cast seed keeps server hitboxes equal to the caster's visuals.
-- System index 1 (2nd) is the shared ArrowHit sub-emitter child (no entry).
skill:addVFX(12, "effects/arrow_rain.json", {
    systems = {
        {
            mode = "Continuous", looping = false, duration = 1.2,
            lifetime = 0.34, speed = 32.0, startSize = 0.3,
            shapeType = "Cone",
            shapePosition = Vec3(0.0, 11.0, 0.0),
            shapeEuler = Vec3(90.0, 0.0, 0.0),
            meshEuler  = Vec3(90.0, 0.0, 0.0),
            coneRadius = 4.75, coneAngle = 0.0, radiusThickness = 1.0,
            maxParticles = 64,
            bursts = { { time = 0.0, count = 3, cycleCount = 10, interval = 0.12 } },
        },
    }
})

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
