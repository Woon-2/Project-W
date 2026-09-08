#ifndef __skill_skillLoadout_HPP
#define __skill_skillLoadout_HPP

// Per-weapon resolved skill loadout for the stack-charge dial HUD and input.
// Built once after SkillCompiler::compileAll() from each asset's loadout
// metadata (weapon / dialSlot / isBasic / chargeCost / cooldownMs).
//
// weaponType ordinal mirrors PlayerWeaponType (ServerEngine/protocol.hpp):
//   Katana(sword)=0, SpearHook(spear)=1, CrystalWand(wand)=2, HeavyArrow(bow)=3.

#include "skillTypes.hpp"

#include <array>
#include <vector>

struct WeaponLoadout {
    int   basicAssetId      = -1;                 // left-click basic attack asset id (-1 = none)
    int   slotAssetId[3]    = { -1, -1, -1 };     // dial slot 0..2 -> compiled asset id
    float slotCost[3]       = { 0.f, 0.f, 0.f };  // charge points required per cast
    float slotCooldownMs[3] = { 0.f, 0.f, 0.f };  // cast cooldown
};

struct SkillLoadout {
    std::array<WeaponLoadout, 4> byWeapon{};  // indexed by PlayerWeaponType ordinal

    static SkillLoadout build(const std::vector<SkillAsset>& assets) {
        SkillLoadout out{};
        for (const SkillAsset& a : assets) {
            if (a.weaponType > 3) continue;  // 0xFF = not in a loadout
            WeaponLoadout& wl = out.byWeapon[a.weaponType];
            if (a.isBasic) {
                wl.basicAssetId = static_cast<int>(a.id);
            } else if (a.loadoutSlot >= 0 && a.loadoutSlot < 3) {
                const int s = a.loadoutSlot;
                wl.slotAssetId[s]    = static_cast<int>(a.id);
                wl.slotCost[s]       = a.chargeCost;
                wl.slotCooldownMs[s] = a.cooldown.count();
            }
        }
        return out;
    }

    const WeaponLoadout& forWeapon(unsigned weaponOrdinal) const {
        return byWeapon[weaponOrdinal < 4 ? weaponOrdinal : 0];
    }

    // charge cost for a (weapon, slot); 0 if out of range (avoids div-by-zero).
    float cost(unsigned weaponOrdinal, int slot) const {
        if (weaponOrdinal >= 4 || slot < 0 || slot >= 3) return 0.f;
        return byWeapon[weaponOrdinal].slotCost[slot];
    }
};

#endif  // __skill_skillLoadout_HPP
