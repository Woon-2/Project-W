-- piercing_slash.lua
-- Foundation skill paired 1:1 with the "Piercing Slash" VFX (vfxId 16).
-- Melee-arc archetype. Tune via the in-game skill editor, then port values back here.

local skill = Skill()
skill.name             = "PiercingSlash"
skill.totalDurationMs  = 500
skill.cooldownMs       = 1000
skill.interruptible    = true

skill:addVFX(16, "effects/piercing_slash.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(100, "PlayVFX", {
    vfxId  = 16,
    offset = Vec3(0.0, 1.0, 0.0)
})

local onHitBase = OnHit({
    damage          = 2,
    vfxId           = 255,
    impulseStrength = 1000.0,
    impulseDir      = Vec3(0.0, 0.3, 1.0)
})

-- Ring layout. attach-local axes: right = X, up = Y, forward = Z.
-- The i-th box sits on a circle of radius `radius` at angle `deg` measured
-- around the up axis from the forward direction, oriented to face outward,
-- and pushes enemies radially outward with a slight upward lift.
local radius    = 2.4
local count     = 8
local spawnMs   = 100
local destroyMs = 250

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
        localOBBs           = { OBB(0.0, 0.75 * radius * s, 0.75 * radius * c, 0.45, 1.5, 1.2, 0, deg, 0) },
        attach              = BoneAttach("spine_01"),
        applyAttachRotation = true,
        hitGroup            = 0,
        hitGroupCooldownMs  = 300,
        onHit               = onHit
    })
    skill:addEvent(destroyMs, "DestroyHitbox", { slot = i })
end

return skill