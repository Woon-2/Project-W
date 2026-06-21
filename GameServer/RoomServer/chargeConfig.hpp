#ifndef room_server_charge_config_hpp
#define room_server_charge_config_hpp

// Stack-charge economy tuning, loaded once at RoomServer startup from
// ../resources/data/chargeConfig.lua. Kept free of sol2 so it can be included
// widely (AssetManager etc.); the Lua parsing lives in chargeConfig.cpp.

#include "protocol.hpp"   // ObjectType
#include "types.hpp"      // Milliseconds, integer aliases

#include <array>
#include <vector>
#include <filesystem>

class ChargeConfig {
public:
    // Parse the Lua config. On any error, keeps built-in defaults and logs.
    void load(const std::filesystem::path& luaPath);

    // Charge points granted per kill for a monster ObjectType (0 if not listed).
    float monsterCharge(ObjectType type) const;

    // Player HP regen per second as a function of the kill-combo count. S-curve
    // (Hill): base at combo 0, staying low until the combo builds and rising to the
    // base->cap midpoint around `halfCombo`, asymptoting to cap.
    float hpRegenPerSec(uint16 comboCount) const;

    // Incoming-charge scale once a slot already holds curStacks full casts.
    float softCapFactor(int curStacks) const;

    Milliseconds damageWindow() const { return damageWindowMs_; }
    Milliseconds comboWindow()  const { return comboWindowMs_; }

private:
    static constexpr std::size_t kMaxObjectTypes = 16;
    std::array<float, kMaxObjectTypes> monsterCharge_{};  // by ObjectType ordinal

    Milliseconds comboWindowMs_ { 3000.f };
    Milliseconds damageWindowMs_{ 15000.f };

    // HP regen accelerator (combo-driven, S-curve). base = no-combo rate, cap = asymptote,
    // halfCombo = combo at the base->cap midpoint, exponent = steepness (>1 = slow start).
    float regenBasePerSec_ = 1.f;
    float regenCapPerSec_  = 25.f;
    float regenHalfCombo_  = 10.f;
    float regenExponent_   = 3.f;

    int   softCapStart_ = 5;
    float softCapDecay_ = 0.3f;
};

#endif // room_server_charge_config_hpp
