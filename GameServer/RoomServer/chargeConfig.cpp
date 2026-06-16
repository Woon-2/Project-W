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
        comboMaxMult_  = static_cast<float>(combo->get_or("maxMult", 2.0));
        if (sol::optional<sol::table> mult = (*combo)["mult"]) {
            std::vector<float> tiers;
            for (std::size_t i = 1; i <= mult->size(); ++i) {
                sol::optional<double> m = (*mult)[i];
                if (m) tiers.push_back(static_cast<float>(*m));
            }
            if (!tiers.empty()) comboMult_ = std::move(tiers);
        }
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

float ChargeConfig::comboMult(uint16 comboCount) const {
    if (comboMult_.empty()) return 1.f;
    const std::size_t idx = (comboCount == 0)
        ? 0
        : std::min<std::size_t>(comboCount - 1, comboMult_.size() - 1);
    return std::min(comboMult_[idx], comboMaxMult_);
}

float ChargeConfig::softCapFactor(int curStacks) const {
    // Flat reduction once at/over the soft cap: charge keeps trickling in so the
    // total asymptotes upward instead of hitting a hard ceiling. The curve can be
    // made progressive later without touching callers.
    return (curStacks >= softCapStart_) ? softCapDecay_ : 1.f;
}
