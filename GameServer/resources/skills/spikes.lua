-- spikes.lua
-- Foundation skill paired 1:1 with the "Spikes" VFX (vfxId 5).
-- Radial PBAoE archetype: 8 hitboxes arranged in a 2m-radius ring around the
-- caster, each knocking enemies outward (radially away from the caster).
-- Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "Spikes"
skill.weapon           = "wand"
skill.isBasic          = true
skill.totalDurationMs  = 800
skill.interruptible    = true

skill:addVFX(5, "effects/spikes.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(150, "PlayVFX", {
    vfxId  = 5,
    offset = Vec3(0.0, 0.0, 0.0),
    groundLock = true
})

-- Quake SFX, as the spikes erupt.
skill:addEvent(150, "PlaySound", { sound = "quake" })

local onHitBase = OnHit({
    damage          = 20,
    vfxId           = 255,
    impulseStrength = 1000.0,
    impulseDir      = Vec3(0.0, 0.3, 1.0)
})

-- Ring layout. attach-local axes: right = X, up = Y, forward = Z.
-- The i-th box sits on a circle of radius `radius` at angle `deg` measured
-- around the up axis from the forward direction, oriented to face outward,
-- and pushes enemies radially outward with a slight upward lift.
local radius    = 2.0
local count     = 8
local spawnMs   = 250
local destroyMs = 600

for i = 0, count - 1 do
    local deg = i * (360.0 / count)
    local rad = math.rad(deg)
    local s   = math.sin(rad)
    local c   = math.cos(rad)

    local onHit = deepCopy(onHitBase)
    onHit.impulseDir = Vec3(c * 0.85, 0.15, -s * 0.85)

    skill:addEvent(spawnMs, "SpawnHitbox", {
        slot                = i,
        -- center on the ring; halfExtents: tangential X, height Y, radial Z.
        localOBBs           = { OBB(0.6, 0.75 * radius * s, 0.75 * radius * c, 0.6, 1.6, 1.2, 0, deg, 0) },
        attach              = BoneAttach("spine_01"),
        applyAttachRotation = true,
        hitGroup            = 0,
        hitGroupCooldownMs  = 600,
        onHit               = onHit
    })
    skill:addEvent(destroyMs, "DestroyHitbox", { slot = i })
end

return skill
