-- sword_slash.lua
-- Basic melee sword slash skill.

local skill = Skill()
skill.name             = "SwordSlash"
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
    damage          = 5,
    vfxId           = 0,
    impulseStrength = 500.0,
    impulseDir      = Vec3(0.0, 0.0, 1.0)
})

local onHitLB = deepCopy(onHitDefBase)
onHitLB.impulseDir = Vec3(-0.6, 0.01, 0.5)
local onHitLF = deepCopy(onHitDefBase)
onHitLF.impulseDir = Vec3(-0.3, 0.01, 0.7)
local onHitF = deepCopy(onHitDefBase)
onHitF.impulseDir = Vec3(0.0, 0.01, 0.9)
local onHitRB = deepCopy(onHitDefBase)
onHitRB.impulseDir = Vec3(0.6, 0.01, 0.5)
local onHitRF = deepCopy(onHitDefBase)
onHitRF.impulseDir = Vec3(0.3, 0.01, 0.7)

local hitboxBase = {
    attach             = BoneAttach("spine_02"),
    applyAttachRotation = true,
    hitGroup           = 0,
    hitGroupCooldownMs = 100,
    onHit              = onHitDefBase
}

-- Left backward sweep segment
skill:addEvent(100, "SpawnHitbox", {
    slot      = 0,
    localOBBs = { OBB(0.3, 0.3, -1.2, 0.15, 0.9, 0.8, 0, 72, 0) },
    attach              = hitboxBase.attach,
    applyAttachRotation = hitboxBase.applyAttachRotation,
    hitGroup            = hitboxBase.hitGroup,
    hitGroupCooldownMs  = hitboxBase.hitGroupCooldownMs,
    onHit               = onHitLB
})

-- Left forward sweep segment
skill:addEvent(120, "SpawnHitbox", {
    slot      = 1,
    localOBBs = { OBB(0.3, -0.9, -0.75, 0.15, 0.7, 0.9, 0, 48, 0) },
    attach              = hitboxBase.attach,
    applyAttachRotation = hitboxBase.applyAttachRotation,
    hitGroup            = hitboxBase.hitGroup,
    hitGroupCooldownMs  = hitboxBase.hitGroupCooldownMs,
    onHit               = onHitLF
})

-- forward sweep segment
skill:addEvent(140, "SpawnHitbox", {
    slot      = 3,
    localOBBs = { OBB(0.3, -1.0, 0.0, 0.15, 0.9, 0.8, 0, 0, 0) },
    attach              = hitboxBase.attach,
    applyAttachRotation = hitboxBase.applyAttachRotation,
    hitGroup            = hitboxBase.hitGroup,
    hitGroupCooldownMs  = hitboxBase.hitGroupCooldownMs,
    onHit               = onHitF
})

-- Right forward sweep segment
skill:addEvent(160, "SpawnHitbox", {
    slot      = 4,
    localOBBs = { OBB(0.3, -0.9, 0.75, 0.15, 0.7, 0.9, 0, -48, 0) },
    attach              = hitboxBase.attach,
    applyAttachRotation = hitboxBase.applyAttachRotation,
    hitGroup            = hitboxBase.hitGroup,
    hitGroupCooldownMs  = hitboxBase.hitGroupCooldownMs,
    onHit               = onHitRF
})

-- Right backward sweep segment
skill:addEvent(180, "SpawnHitbox", {
    slot      = 5,
    localOBBs = { OBB(0.3, 0.3, 1.2, 0.15, 0.9, 0.8, 0, -72, 0) },
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

skill:addEvent(200, "CameraShake", {
    magnitude  = 0.3,
    durationMs = 100
})

return skill
