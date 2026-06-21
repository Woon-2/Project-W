-- piercing_circle_slash.lua
-- Foundation skill paired 1:1 with the "Piercing Circle Slash" VFX (vfxId 17).
-- Melee-arc archetype (surround). Tune via the in-game skill editor, then port values back here.

local skill = Skill()
skill.name             = "PiercingCircleSlash"
skill.weapon           = "spear"
skill.dialSlot         = 1
skill.chargeCost       = 6
skill.cooldownMs       = 1400
skill.totalDurationMs  = 1000
skill.interruptible    = true

skill:addVFX(0, "effects/blood_hit.json")
skill:addVFX(17, "effects/piercing_circle_slash.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(100, "PlayVFX", {
    vfxId  = 17,
    offset = Vec3(0.0, 1.0, 0.0)
})

-- Spear wide circle-slash SFX, on the sweep.
skill:addEvent(100, "PlaySound", { sound = "spear3" })

local onHitBase = OnHit({
    damage          = 4,
    vfxId           = 0,
    impulseStrength = 350.0,
    impulseDir      = Vec3(0.0, 0.0, 1.0)
})

-- Ring layout. attach-local axes: right = X, up = Y, forward = Z.
-- The i-th box sits on a circle of radius `radius` at angle `deg` measured
-- around the up axis from the forward direction, oriented to face outward,
-- and pushes enemies radially outward with a slight upward lift.
local radius    = 5.4
local count     = 8
local spawnMs   = 120
local destroyMs = 850

for i = 0, count - 1 do
    local deg = i * (360.0 / count)
    local rad = math.rad(deg)
    local s   = math.sin(rad)
    local c   = math.cos(rad)

    local onHit = deepCopy(onHitBase)
    onHit.impulseDir = Vec3(c, 0.0, -s)

    skill:addEvent(spawnMs, "SpawnHitbox", {
        slot                = i,
        -- center on the ring; halfExtents: tangential X, height Y, radial Z.
        localOBBs           = { OBB(0.0, 0.75 * radius * s, 0.75 * radius * c, 0.5, 2.7, 2.4, 0, deg, 0) },
        attach              = BoneAttach("spine_01"),
        applyAttachRotation = true,
        hitGroup            = 0,
        hitGroupCooldownMs  = 250,
        onHit               = onHit
    })
    skill:addEvent(destroyMs, "DestroyHitbox", { slot = i })
end

return skill