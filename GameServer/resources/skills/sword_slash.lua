-- sword_slash.lua
-- Basic melee sword slash skill.

local skill = Skill()
skill.name             = "SwordSlash"
skill.weapon           = "sword"
skill.isBasic          = true
skill.totalDurationMs  = 700
skill.interruptible    = true

skill:addVFX(0, "effects/blood_hit.json")
skill:addVFX(1, "effects/sword_slash_1.json")

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Combat_2H_Attack",
    attackIndex = 0,
    blendTime   = 0.1
})

skill:addEvent(250, "PlayVFX", {
    vfxId  = 1,
    offset = Vec3(0.0, 0.8, 1.0),
    flipX  = true   -- mirror the slash arc to match the right-to-left swing
})

-- Swing SFX, on the slash.
skill:addEvent(350, "PlaySound", { sound = "sword_slash_1" })

local onHitDefBase = OnHit({
    damage          = 200,
    vfxId           = 0,
    impulseStrength = 700.0,
    impulseDir      = Vec3(0.0, 0.0, 1.0)
})

local onHitLB = deepCopy(onHitDefBase)
onHitLB.impulseDir = Vec3(-0.97, 0.1, 0.2)
local onHitLF = deepCopy(onHitDefBase)
onHitLF.impulseDir = Vec3(-0.52, 0.1, 0.85)
local onHitF = deepCopy(onHitDefBase)
onHitF.impulseDir = Vec3(0.0, 0.1, 0.99)
local onHitRB = deepCopy(onHitDefBase)
onHitRB.impulseDir = Vec3(0.97, 0.1, 0.2)
local onHitRF = deepCopy(onHitDefBase)
onHitRF.impulseDir = Vec3(0.52, 0.1, 0.85)

local hitboxBase = {
    attach             = BoneAttach("spine_01"),
    applyAttachRotation = true,
    hitGroup           = 0,
    hitGroupCooldownMs = 350,
    onHit              = onHitDefBase
}

-- Right backward sweep segment
skill:addEvent(200, "SpawnHitbox", {
    slot      = 0,
    localOBBs = { OBB(0.25, -1.45, 0.3, 0.15, 1.1, 0.8, 0, 0, 0) },
    attach              = hitboxBase.attach,
    applyAttachRotation = hitboxBase.applyAttachRotation,
    hitGroup            = hitboxBase.hitGroup,
    hitGroupCooldownMs  = hitboxBase.hitGroupCooldownMs,
    onHit               = onHitRB
})

-- Right forward sweep segment
skill:addEvent(220, "SpawnHitbox", {
    slot      = 1,
    localOBBs = { OBB(1.4, -0.75, 0.3, 0.15, 1.3, 1.2, 0, 0, 0) },
    attach              = hitboxBase.attach,
    applyAttachRotation = hitboxBase.applyAttachRotation,
    hitGroup            = hitboxBase.hitGroup,
    hitGroupCooldownMs  = hitboxBase.hitGroupCooldownMs,
    onHit               = onHitRF
})

-- forward sweep segment
skill:addEvent(240, "SpawnHitbox", {
    slot      = 2,
    localOBBs = { OBB(1.6, 0.0, 0.3, 0.15, 1.55, 1.0, 0, 0, 0) },
    attach              = hitboxBase.attach,
    applyAttachRotation = hitboxBase.applyAttachRotation,
    hitGroup            = hitboxBase.hitGroup,
    hitGroupCooldownMs  = hitboxBase.hitGroupCooldownMs,
    onHit               = onHitF
})

-- Left forward sweep segment
skill:addEvent(260, "SpawnHitbox", {
    slot      = 3,
    localOBBs = { OBB(1.4, 0.75, 0.3, 0.15, 1.3, 1.2, 0, 0, 0) },
    attach              = hitboxBase.attach,
    applyAttachRotation = hitboxBase.applyAttachRotation,
    hitGroup            = hitboxBase.hitGroup,
    hitGroupCooldownMs  = hitboxBase.hitGroupCooldownMs,
    onHit               = onHitLF
})

-- Left backward sweep segment
skill:addEvent(280, "SpawnHitbox", {
    slot      = 4,
    localOBBs = { OBB(0.25, 1.45, 0.3, 0.15, 1.1, 0.8, 77, 0, 0) },
    attach              = hitboxBase.attach,
    applyAttachRotation = hitboxBase.applyAttachRotation,
    hitGroup            = hitboxBase.hitGroup,
    hitGroupCooldownMs  = hitboxBase.hitGroupCooldownMs,
    onHit               = onHitLB
})

skill:addEvent(400, "DestroyHitbox", { slot = 0 })
skill:addEvent(420, "DestroyHitbox", { slot = 1 })
skill:addEvent(440, "DestroyHitbox", { slot = 2 })
skill:addEvent(460, "DestroyHitbox", { slot = 3 })
skill:addEvent(480, "DestroyHitbox", { slot = 4 })

return skill
