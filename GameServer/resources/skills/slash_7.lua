-- slash_7.lua
-- Foundation skill paired 1:1 with the "Slash 7" VFX (vfxId 4).
-- Radial PBAoE archetype: 8 hitboxes arranged in a 2m-radius ring around the
-- caster, each knocking enemies outward (radially away from the caster).
-- Tune via the in-game skill editor, then port values back here.

local skill = Skill()
skill.name             = "Slash7"
skill.weapon           = "sword"
skill.dialSlot         = 1
skill.chargeCost       = 5
skill.cooldownMs       = 800
skill.totalDurationMs  = 700
skill.interruptible    = true

skill:addVFX(0, "effects/blood_hit.json")
skill:addVFX(4, "effects/slash_7.json")

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Combat_2H_AttackSpecial",
    attackIndex = 2,
    blendTime   = 0.1
})

skill:addEvent(400, "PlayVFX", {
    vfxId  = 4,
    offset = Vec3(0.0, 1.0, 0.0)
})

-- Radial burst SFX, on the slash.
skill:addEvent(450, "PlaySound", { sound = "sword_slash_7" })

local onHitBase = OnHit({
    damage          = 45,
    vfxId           = 0,
    impulseStrength = 1000.0,
    impulseDir      = Vec3(0.0, 0.3, 1.0)
})

-- Ring layout. attach-local axes: right = X, up = Y, forward = Z.
-- The i-th box sits on a circle of radius `radius` at angle `deg` measured
-- around the up axis from the forward direction, oriented to face outward,
-- and pushes enemies radially outward with a slight upward lift.
local radius    = 3.0
local count     = 8
local spawnMs   = 450
local destroyMs = 600

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
        localOBBs           = { OBB(0.0, 0.75 * radius * s, 0.75 * radius * c, 0.45, 1.8, 1.5, 0, deg, 0) },
        attach              = BoneAttach("spine_01"),
        applyAttachRotation = true,
        hitGroup            = 0,
        hitGroupCooldownMs  = 300,
        onHit               = onHit
    })
    skill:addEvent(destroyMs, "DestroyHitbox", { slot = i })
end

return skill
