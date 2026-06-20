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
        Bomber = 10,
        Birdy = 10,
        Slime = 10,
        Treant = 10
    },

    -- A player must have dealt damage to the monster within this window (ms)
    -- before its death to be credited.
    damageWindowMs = 15000,

    -- Consecutive-kill combo. `windowMs` is the time allowed between credited
    -- kills before the combo resets. The combo count now drives the player's HP
    -- regen rate (see `regen` below), not charge gain.
    combo = {
        windowMs = 3000,
    },

    -- Player HP regen per second as an S-curve of the kill-combo count:
    --   y = basePerSec + (capPerSec - basePerSec) * x^exponent / (x^exponent + halfCombo^exponent)
    -- `basePerSec` with no combo, staying low until the streak builds, crossing the
    -- base->cap midpoint at `halfCombo` and asymptoting to `capPerSec`. Higher
    -- `exponent` (>1) means a flatter start and a sharper rise (a higher threshold).
    regen = {
        basePerSec = 1.0,
        capPerSec  = 25.0,
        halfCombo  = 10.0,
        exponent   = 3.0,
    },

    -- Soft cap: once a slot already holds at least `startStacks` full casts,
    -- incoming charge is scaled by `decay` so accumulation asymptotes rather
    -- than growing without bound.
    softCap = {
        startStacks = 5,
        decay       = 0.3,
    },
}
