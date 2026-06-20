-- chargeConfig.lua
-- Stack-charge economy tuning, loaded once by the RoomServer at startup
-- (RoomServer/chargeConfig.cpp). Separate from the skill system on purpose:
-- monster -> charge mapping is gameplay data, not skill metadata. Values here
-- are placeholders meant to be balanced later.

return {
    -- Charge points granted per kill, keyed by ObjectType name. Bosses can
    -- override this per-spawn on the server.
    monsters = {
        Goblin   = 10,
        Snake    = 10,
        Mushroom = 10,
        Birdy = 10,
        Bomber = 10,
        Slime = 10,
        Treant = 10
    },

    -- A player must have dealt damage to the monster within this window (ms)
    -- before its death to be credited.
    damageWindowMs = 15000,

    -- Consecutive-kill combo accelerator. `mult` is indexed by combo count
    -- (entry 1 = first kill); the last entry repeats for higher counts. The
    -- result is clamped to `maxMult`. `windowMs` is the time allowed between
    -- credited kills before the combo resets.
    combo = {
        windowMs = 3000,
        maxMult  = 2.0,
        mult     = { 1.0, 1.0, 1.25, 1.5, 1.75 },
    },

    -- Soft cap: once a slot already holds at least `startStacks` full casts,
    -- incoming charge is scaled by `decay` so accumulation asymptotes rather
    -- than growing without bound.
    softCap = {
        startStacks = 5,
        decay       = 0.3,
    },
}
