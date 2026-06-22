-- slash_combo.lua
-- Foundation skill paired 1:1 with the "Slash Combo" VFX (vfxId 3).
-- Melee-arc archetype. Tune via the in-game skill editor, then port values back here.

local skill = Skill()
skill.name             = "SlashCombo"
skill.weapon           = "sword"
skill.dialSlot         = 2
skill.chargeCost       = 8
skill.cooldownMs       = 2400
skill.totalDurationMs  = 2000
skill.interruptible    = true

skill:addVFX(0, "effects/blood_hit.json")
skill:addVFX(3, "effects/slash_combo.json")
  
-- 3단 콤보: 히트 웨이브(≈150 / 550 / 1250ms)에 맞춰 공격 클립을 순차 전환한다.
skill:addEvent(0, "PlayAnimation", {
    clipName    = "Combat_2H_Attack",
    attackIndex = 0,
    blendTime   = 0.1
})
skill:addEvent(400, "PlayAnimation", {
    clipName    = "Combat_2H_Attack01",
    attackIndex = 1,
    blendTime   = 0.1
})
skill:addEvent(800, "PlayAnimation", {
    clipName    = "Combat_2H_AttackSpecial",
    attackIndex = 2,
    blendTime   = 0.1
})

skill:addEvent(100, "PlayVFX", {
    vfxId  = 3,
    offset = Vec3(0.0, 1.0, 0.0)
})

-- Per-beat swing SFX, aligned to the four hitbox waves (SpawnHitbox times):
--   150 / 550 / 750ms = light slashes; 1250ms = finisher (the big double-arc wave).
skill:addEvent(150,  "PlaySound", { sound = "sword_slash_1" })
skill:addEvent(350,  "PlaySound", { sound = "sword_slash_1" })
skill:addEvent(550,  "PlaySound", { sound = "sword_slash_1" })
skill:addEvent(1000, "PlaySound", { sound = "sword_slash_finish" })

local onHit = OnHit({
    damage          = 35,
    vfxId           = 0,
    impulseStrength = 700.0,
    impulseDir      = Vec3(0.0, 0.1, 1.0)
})

-- 피니셔(1250ms 더블 아크, slot 10/11)는 더 큰 혈흔으로 극적인 마무리 연출.
local onHitFinisher = deepCopy(onHit)
onHitFinisher.vfxScale = 2.5

skill:addEvent(150, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(-0.25, 0.05, -1.45, 0.55, 1.2, 1.4, 29, 80, -8) },
    attach              = BoneAttach("spine_01"),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 300,
    onHit               = onHit
})

skill:addEvent(150, "SpawnHitbox", {
    slot                = 1,
    localOBBs           = { OBB(0.2, -1.2, -0.65, 0.55, 1.15, 1.6, 37, 31, 5) },
    attach              = BoneAttach("spine_01"),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 300,
    onHit               = onHit
})

skill:addEvent(150, "SpawnHitbox", {
    slot                = 2,
    localOBBs           = { OBB(1.1, -1.5, 0.7, 0.55, 1.0, 1.4, 29, 80, 0) },
    attach              = BoneAttach("spine_01"),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 300,
    onHit               = onHit
})

skill:addEvent(550, "SpawnHitbox", {
    slot                = 3,
    localOBBs           = { OBB(0.0, 0.35, -1.6, 0.3, 1.5, 1.2, -8, 44, 4) },
    attach              = BoneAttach("spine_01"),
    applyAttachRotation = true,
    hitGroup            = 1,
    hitGroupCooldownMs  = 300,
    onHit               = onHit
})

skill:addEvent(550, "SpawnHitbox", {
    slot                = 4,
    localOBBs           = { OBB(0.0, -0.4, -0.35, 0.3, 1.7, 1.2, -8, 13, 4) },
    attach              = BoneAttach("spine_01"),
    applyAttachRotation = true,
    hitGroup            = 1,
    hitGroupCooldownMs  = 300,
    onHit               = onHit
})

skill:addEvent(550, "SpawnHitbox", {
    slot                = 5,
    localOBBs           = { OBB(0.0, -0.55, 1.1, 0.3, 1.65, 1.1, -8, -6, 4) },
    attach              = BoneAttach("spine_01"),
    applyAttachRotation = true,
    hitGroup            = 1,
    hitGroupCooldownMs  = 300,
    onHit               = onHit
})

skill:addEvent(750, "SpawnHitbox", {
    slot                = 6,
    localOBBs           = { OBB(0.0, 0.85, -1.6, 0.3, 1.25, 0.8, 0, 67, -17) },
    attach              = BoneAttach("spine_01"),
    applyAttachRotation = true,
    hitGroup            = 2,
    hitGroupCooldownMs  = 300,
    onHit               = onHit
})

skill:addEvent(750, "SpawnHitbox", {
    slot                = 7,
    localOBBs           = { OBB(0.0, -0.15, -0.75, 0.3, 1.55, 1.2, 5, 43, -15) },
    attach              = BoneAttach("spine_01"),
    applyAttachRotation = true,
    hitGroup            = 2,
    hitGroupCooldownMs  = 300,
    onHit               = onHit
})

skill:addEvent(750, "SpawnHitbox", {
    slot                = 8,
    localOBBs           = { OBB(0.0, -0.4, 0.2, 0.3, 1.5, 1.2, 10, -7, -10) },
    attach              = BoneAttach("spine_01"),
    applyAttachRotation = true,
    hitGroup            = 2,
    hitGroupCooldownMs  = 300,
    onHit               = onHit
})

skill:addEvent(750, "SpawnHitbox", {
    slot                = 9,
    localOBBs           = { OBB(0.05, 0.1, 1.45, 0.3, 1.5, 1.2, 6, -38, -5) },
    attach              = BoneAttach("spine_01"),
    applyAttachRotation = true,
    hitGroup            = 3,
    hitGroupCooldownMs  = 300,
    onHit               = onHit
})

skill:addEvent(900, "SpawnHitbox", {
    slot                = 10,
    localOBBs           = { OBB(0.4, -0.95, -0.75, 0.65, 1.15, 2.3, 0, 50, 0) },
    attach              = BoneAttach("spine_01"),
    applyAttachRotation = true,
    hitGroup            = 3,
    hitGroupCooldownMs  = 300,
    onHit               = onHitFinisher
})

skill:addEvent(900, "SpawnHitbox", {
    slot                = 11,
    localOBBs           = { OBB(0.4, 1.1, 0.7, 0.65, 1.15, 2.3, 0, 50, 0) },
    attach              = BoneAttach("spine_01"),
    applyAttachRotation = true,
    hitGroup            = 3,
    hitGroupCooldownMs  = 300,
    onHit               = onHitFinisher
})

skill:addEvent(350, "DestroyHitbox", { slot = 0 })
skill:addEvent(350, "DestroyHitbox", { slot = 1 })
skill:addEvent(350, "DestroyHitbox", { slot = 2 })
skill:addEvent(700, "DestroyHitbox", { slot = 3 })
skill:addEvent(700, "DestroyHitbox", { slot = 4 })
skill:addEvent(700, "DestroyHitbox", { slot = 5 })
skill:addEvent(900, "DestroyHitbox", { slot = 6 })
skill:addEvent(900, "DestroyHitbox", { slot = 7 })
skill:addEvent(900, "DestroyHitbox", { slot = 8 })
skill:addEvent(900, "DestroyHitbox", { slot = 9 })
skill:addEvent(1100, "DestroyHitbox", { slot = 10 })
skill:addEvent(1100, "DestroyHitbox", { slot = 11 })

return skill
