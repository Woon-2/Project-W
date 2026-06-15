-- piercing_multi.lua
-- Foundation skill paired 1:1 with the "Piercing Multi" VFX (vfxId 18).
-- Multi-projectile archetype (wide forward box). Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "PiercingMulti"
skill.weapon           = "spear"
skill.dialSlot         = 2
skill.chargeCost       = 8
skill.cooldownMs       = 1500
skill.totalDurationMs  = 1100
skill.interruptible    = true

-- systems[1..20] = stab systems (code-built; no JSON source). Mirrors client
-- game.cpp: 10 waves x 2 lanes, wave interval 0.06s, lifetime 0.42, speed 0,
-- local +Z constant velocity 25 (volLinear), box spawn jitter 0.56x0.42,
-- mesh forward fix yaw -90 (Unity asset fires along local +X).
-- System order: wave-major, lane inner (index = wave*2 + lane).
local pmOffsets = {
    { -2.24,  1.04 }, {  0.88, -0.62 },
    { -0.62, -1.08 }, {  2.18,  0.38 },
    { -1.78,  0.24 }, {  1.46,  1.12 },
    { -2.38, -0.72 }, {  0.34,  0.82 },
    { -0.96,  1.18 }, {  2.34, -0.24 },
    { -1.42, -0.38 }, {  0.72, -1.16 },
    { -2.08,  0.62 }, {  1.88,  0.92 },
    { -0.28, -0.84 }, {  2.48, -1.02 },
    { -2.46,  0.06 }, {  0.18,  1.24 },
    { -1.12, -1.22 }, {  1.24,  0.14 },
}
local pmSystems = {}
for i = 1, 20 do
    local wave = ((i - 1) - (i - 1) % 2) / 2
    pmSystems[i] = {
        -- JSON base (startSize3D / startRotation3D randomization lives there),
        -- then the same overrides game.cpp applies on top of the imported cfg.
        name = "PS_VFX_Piercing",
        mode = "Continuous", looping = false,
        duration = 0.64,         -- 0.06 * 9 + 0.1 (client cfg.main.duration)
        lifetime = 0.42, speed = 0.0,
        maxParticles = 4,
        shapeType = "Box",
        shapePosition = Vec3(pmOffsets[i][1], pmOffsets[i][2], 0.0),
        shapeEuler = Vec3(0.0, 0.0, 0.0),   -- client sets shape.orientation = identity
        boxSize = Vec3(0.56, 0.42, 0.0),
        volLinear = Vec3(0.0, 0.0, 25.0),
        meshEuler = Vec3(0.0, -90.0, 0.0),  -- piercingForwardFix (local +X -> +Z)
        emitRate = 0.0,
        bursts = { { time = 0.06 * wave, count = 1, cycleCount = 1 } },
    }
end
skill:addVFX(18, "effects/PS_VFX_Piercing_ParticleSystems.json", { systems = pmSystems })

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
