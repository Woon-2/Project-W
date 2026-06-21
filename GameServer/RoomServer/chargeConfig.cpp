#include "rspch.hpp"
#include "chargeConfig.hpp"

#include <sol/sol.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>

static ObjectType parseObjectType(const std::string& s) {
    if (s == "Player")     return ObjectType::Player;
    if (s == "Goblin")     return ObjectType::Goblin;
    if (s == "Ground")     return ObjectType::Ground;
    if (s == "Stronghold") return ObjectType::Stronghold;
    if (s == "Snake")      return ObjectType::Snake;
    if (s == "Mushroom")   return ObjectType::Mushroom;
    if (s == "Hobgoblin")  return ObjectType::Hobgoblin;
    if (s == "Bomber")     return ObjectType::Bomber;
    if (s == "Birdy")      return ObjectType::Birdy;
    if (s == "Slime")      return ObjectType::Slime;
    if (s == "Treant")     return ObjectType::Treant;
    if (s == "Grandbaum")  return ObjectType::Grandbaum;
    if (s == "Isys")       return ObjectType::Isys;
    return ObjectType::Goblin;
}

void ChargeConfig::load(const std::filesystem::path& luaPath) {
    sol::state lua;
    lua.open_libraries(sol::lib::base, sol::lib::table, sol::lib::math);

    sol::protected_function_result res;
    try {
        res = lua.safe_script_file(luaPath.string());
    } catch (const std::exception& e) {
        std::cerr << "[ChargeConfig] load failed (" << e.what() << ") -- using defaults\n";
        return;
    }
    if (!res.valid() || res.get_type() != sol::type::table) {
        std::cerr << "[ChargeConfig] '" << luaPath.string() << "' did not return a table -- using defaults\n";
        return;
    }
    sol::table t = res;

    if (sol::optional<sol::table> mons = t["monsters"]) {
        mons->for_each([&](sol::object k, sol::object v) {
            if (!k.is<std::string>() || !v.is<double>()) return;
            const std::size_t idx = static_cast<std::size_t>(parseObjectType(k.as<std::string>()));
            if (idx < monsterCharge_.size())
                monsterCharge_[idx] = static_cast<float>(v.as<double>());
        });
    }

    damageWindowMs_ = Milliseconds{ static_cast<float>(t.get_or("damageWindowMs", 15000.0)) };

    if (sol::optional<sol::table> combo = t["combo"]) {
        comboWindowMs_ = Milliseconds{ static_cast<float>(combo->get_or("windowMs", 3000.0)) };
    }

    if (sol::optional<sol::table> regen = t["regen"]) {
        regenBasePerSec_ = static_cast<float>(regen->get_or("basePerSec", 1.0));
        regenCapPerSec_  = static_cast<float>(regen->get_or("capPerSec", 25.0));
        regenHalfCombo_  = static_cast<float>(regen->get_or("halfCombo", 10.0));
        regenExponent_   = static_cast<float>(regen->get_or("exponent", 3.0));
    }

    if (sol::optional<sol::table> sc = t["softCap"]) {
        softCapStart_ = sc->get_or("startStacks", 5);
        softCapDecay_ = static_cast<float>(sc->get_or("decay", 0.3));
    }

    std::cout << "[ChargeConfig] loaded (Goblin charge="
              << monsterCharge(ObjectType::Goblin) << ")\n";
}

float ChargeConfig::monsterCharge(ObjectType type) const {
    const std::size_t idx = static_cast<std::size_t>(type);
    return idx < monsterCharge_.size() ? monsterCharge_[idx] : 0.f;
}

float ChargeConfig::hpRegenPerSec(uint16 comboCount) const {
    // S-curve (Hill): y = base + (cap - base) * x^n / (x^n + halfCombo^n). Stays near
    // base for low combo, crosses the base->cap midpoint at x = halfCombo, asymptotes to cap.
    const float x  = static_cast<float>(comboCount);
    const float xn = std::pow(x, regenExponent_);
    const float mn = std::pow(regenHalfCombo_, regenExponent_);
    const float denom = xn + mn;
    const float t  = (denom > 0.f) ? xn / denom : 0.f;
    return regenBasePerSec_ + (regenCapPerSec_ - regenBasePerSec_) * t;
}

float ChargeConfig::softCapFactor(int curStacks) const {
    // Flat reduction once at/over the soft cap: charge keeps trickling in so the
    // total asymptotes upward instead of hitting a hard ceiling. The curve can be
    // made progressive later without touching callers.
    return (curStacks >= softCapStart_) ? softCapDecay_ : 1.f;
}
