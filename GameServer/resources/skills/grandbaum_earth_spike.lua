-- grandbaum_earth_spike.lua
-- Grandbaum's ranged attack during the ShieldWall phase, where the slime wall keeps every
-- player outside the ring and melee can never reach them.
--
-- Targeted ground skill: the server picks one living player, plants the CAST ANCHOR on them
-- (Room::skillStartInternal(..., castAnchorPos)) and relays it via S_SkillStart::castAnchor*.
-- Everything here is anchored -- the telegraph, the pillar and the hitbox all resolve against
-- that same frame, so the client's prediction and the server's judgement agree.
--
-- The anchor does NOT track: once the telegraph is up the impact point is fixed, so walking
-- out of it is a clean dodge. Keep the ~800ms gap between the telegraph and the eruption --
-- that gap IS the counterplay.
--
-- ART NOTE: vfxId 19 = amber magic circle (magic_circle.dds), vfxId 20 = brown earth spike
-- (IceSpikes2 mesh + MatTwoSides). Both are built in onlineGame.cpp / standalone/game.cpp.
-- The blue crystal art was NOT usable: tint is a multiply, and CrystalFree1.dds is fully
-- saturated blue (mean R 0.03), so a brown multiplier only ever produces near-black.
--
-- INVARIANT: totalDurationMs must stay BELOW the tactic's barrage interval
-- (GrandbaumMidBossTactic::SHIELDWALL_BARRAGE_INTERVAL). skillStartInternal drops a cast
-- silently while the previous instance is still live (hasActiveSkill guard), so an overlong
-- skill would quietly halve the barrage rate.

local skill = Skill()
skill.name            = "Grandbaum_EarthSpike"
skill.totalDurationMs = 1400
-- Not interruptible: the telegraph promises an impact, so it must always resolve.
skill.interruptible   = false

-- Reuses the Treant rig's clap (attackClips_[1] = "Treant_Clap"); Grandbaum shares that
-- clip set, and the server registers it under the clip key "Clap".
skill:addEvent(0, "PlayAnimation", {
    clipName    = "Treant_Clap",
    attackIndex = 1,
    blendTime   = 0.1
})

-- Telegraph: a flat amber magic circle on the target's feet. Lifted slightly off the surface
-- to avoid z-fighting. No groundAlign here: the circle is a WORLD-ALIGNED billboard, and that
-- alignment reads only the particle's own startRotation3D (laid flat in C++), never the
-- effect's play orientation -- so slope tilt has to be skipped, not requested.
skill:addEvent(300, "PlayVFX", {
    vfxId      = 19,
    attach     = GroundAttach{},
    offset     = Vec3(0.0, 0.05, 0.0),
    groundLock = true,
    groundSnap = true
})

skill:addEvent(300, "PlaySound", { sound = "quake" })

-- Eruption: a brown earth spike bursts out of the ground at the anchor. This one IS a mesh
-- particle, so SnapAndAlign genuinely tilts it to the slope (billboards cannot do that).
skill:addEvent(1100, "PlayVFX", {
    vfxId           = 20,
    attach          = GroundAttach{},
    offset          = Vec3(0.0, 0.0, 0.0),
    groundLock      = true,
    groundSnap      = true,
    groundAlign     = true,
    particleConform = "SnapAndAlign"
})

-- Static planted OBB (Ground attach, distributed mode = snaps to the terrain under the
-- anchor). center.y is the lift of the box centre above that surface point, so this spans
-- roughly ground-0.2 .. ground+2.6.
skill:addEvent(1100, "SpawnHitbox", {
    slot                = 0,
    localOBBs           = { OBB(0.0, 1.2, 0.0, 0.8, 1.4, 0.8, 0, 0, 0) },
    attach              = GroundAttach{ align = false },
    applyAttachRotation = true,
    hitGroup            = 0,
    hitGroupCooldownMs  = 0,
    -- damage is the raw number: the tactic casts with damageScale 1.0 on purpose, so this is
    -- NOT multiplied by the boss's attackDamageScale (5.0). Player max HP is 5000.
    -- Upward impulse lands client-side (the local player is the only Dynamic player body),
    -- which is where player position authority lives.
    onHit               = OnHit({
        damage          = 50,
        impulseStrength = 600.0,
        impulseDir      = Vec3(0.0, 0.8, 0.2)
    })
})

skill:addEvent(1400, "DestroyHitbox", { slot = 0 })

return skill
