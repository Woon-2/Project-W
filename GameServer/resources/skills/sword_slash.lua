-- sword_slash.lua
-- Basic melee sword slash skill.

local skill = Skill()
skill.name             = "SwordSlash"
skill.weapon           = "sword"
skill.isBasic          = true
skill.totalDurationMs  = 400
skill.interruptible    = true

skill:addVFX(0, "effects/blood_hit.json")
skill:addVFX(1, "effects/sword_slash_1.json")

skill:addEvent(0, "PlayAnimation", {
    clipName  = "Player_Attack",
    blendTime = 0.1
})

skill:addEvent(100, "PlayVFX", {
    vfxId  = 1,
    offset = Vec3(0.0, 0.8, 1.0)
})

local onHitDefBase = OnHit({
    damage          = 25,
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
    hitGroupCooldownMs = 600,
    onHit              = onHitDefBase
}

-- Left backward sweep segment
skill:addEvent(100, "SpawnHitbox", {
    slot      = 0,
    localOBBs = { OBB(0.3, -0.25, -1.45, 0.15, 1.1, 0.8, 0, 77, 0) },
    attach              = hitboxBase.attach,
    applyAttachRotation = hitboxBase.applyAttachRotation,
    hitGroup            = hitboxBase.hitGroup,
    hitGroupCooldownMs  = hitboxBase.hitGroupCooldownMs,
    onHit               = onHitLB
})

-- Left forward sweep segment
skill:addEvent(120, "SpawnHitbox", {
    slot      = 1,
    localOBBs = { OBB(0.3, -1.4, -0.75, 0.15, 1.3, 1.2, 0, 48, 0) },
    attach              = hitboxBase.attach,
    applyAttachRotation = hitboxBase.applyAttachRotation,
    hitGroup            = hitboxBase.hitGroup,
    hitGroupCooldownMs  = hitboxBase.hitGroupCooldownMs,
    onHit               = onHitLF
})

-- forward sweep segment
skill:addEvent(140, "SpawnHitbox", {
    slot      = 2,
    localOBBs = { OBB(0.3, -1.6, 0.0, 0.15, 1.55, 1.0, 0, 0, 0) },
    attach              = hitboxBase.attach,
    applyAttachRotation = hitboxBase.applyAttachRotation,
    hitGroup            = hitboxBase.hitGroup,
    hitGroupCooldownMs  = hitboxBase.hitGroupCooldownMs,
    onHit               = onHitF
})

-- Right forward sweep segment
skill:addEvent(160, "SpawnHitbox", {
    slot      = 3,
    localOBBs = { OBB(0.3, -1.4, 0.75, 0.15, 1.3, 1.2, 0, -48, 0) },
    attach              = hitboxBase.attach,
    applyAttachRotation = hitboxBase.applyAttachRotation,
    hitGroup            = hitboxBase.hitGroup,
    hitGroupCooldownMs  = hitboxBase.hitGroupCooldownMs,
    onHit               = onHitRF
})

-- Right backward sweep segment
skill:addEvent(180, "SpawnHitbox", {
    slot      = 4,
    localOBBs = { OBB(0.3, -0.25, 1.45, 0.15, 1.1, 0.8, 0, -77, 0) },
    attach              = hitboxBase.attach,
    applyAttachRotation = hitboxBase.applyAttachRotation,
    hitGroup            = hitboxBase.hitGroup,
    hitGroupCooldownMs  = hitboxBase.hitGroupCooldownMs,
    onHit               = onHitRB
})

skill:addEvent(300, "DestroyHitbox", { slot = 0 })
skill:addEvent(320, "DestroyHitbox", { slot = 1 })
skill:addEvent(340, "DestroyHitbox", { slot = 2 })
skill:addEvent(360, "DestroyHitbox", { slot = 3 })
skill:addEvent(380, "DestroyHitbox", { slot = 4 })

return skill
