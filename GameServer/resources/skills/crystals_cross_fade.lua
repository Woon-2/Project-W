-- crystals_cross_fade.lua
-- Foundation skill paired 1:1 with the "Crystals Cross Fade" VFX (vfxId 9).
-- Ground-AoE archetype (surround). Tune via the in-game skill editor.

local skill = Skill()
skill.name             = "CrystalsCrossFade"
skill.weapon           = "wand"
skill.dialSlot         = 1
skill.chargeCost       = 6
skill.cooldownMs       = 1420
skill.totalDurationMs  = 1400
skill.interruptible    = true

-- systems[]: ParticleEffect composition (index = addSystem order in game.cpp).
-- Crystals(1) is a Birth-chained sub-emitter of the trigger system(0): each
-- trigger particle replays the Crystals JSON bursts from its spawn position.
skill:addVFX(9, "effects/Crystals crossfade 2_ParticleSystems.json", {
    systems = {
        { name = "Crystals crossfade 2",          mode = "Continuous", looping = false },
        { name = "Crystals crossfade 2/Crystals", parent = 0, parentEvent = "Birth" },
    }
})

skill:addEvent(0, "PlayAnimation", {
    clipName    = "Combat_Defend_AttackSpecial",
    attackIndex = 2,
    blendTime   = 0.1
})

skill:addEvent(150, "PlayVFX", {
    vfxId  = 9,
    offset = Vec3(0.0, 0.0, 0.0),
    groundLock = true,
    groundSnap = true,
    particleConform = "SnapAndAlign"
})

-- Ice crystals rise SFX, with the VFX.
skill:addEvent(150, "PlaySound", { sound = "ice_crossfade" })

local onHit = OnHit({
    damage          = 98,
    vfxId           = 255,
    impulseStrength = 600.0,
    impulseDir      = Vec3(0.0, 0.7, 0.5)
})

skill:addEvent(160, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, 0.0, 0.0, 0.4, 1.2, 0.4, 0, 0, 0) },
    attach              = VFXParticleAttach(9, 1),
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 1300,
    useParticleSize     = false,
    onHit               = onHit
})

skill:addEvent(1300, "DestroyHitbox", { slot = 0 })

return skill