#ifndef __rs_skill_skillLoadout_HPP
#define __rs_skill_skillLoadout_HPP

// Server mirror of client/skill/skillLoadout.hpp. Resolves each weapon's basic
// attack + 3 dial skills (and their charge cost) from compiled SkillAsset
// metadata. Built once after compileAll(); used for soft-cap stack accounting.
//
// weaponType ordinal mirrors PlayerWeaponType (protocol.hpp):
//   Katana(sword)=0, SpearHook(spear)=1, CrystalWand(wand)=2, HeavyArrow(bow)=3.

#include "skillTypes.hpp"

#include <array>
#include <vector>

struct WeaponLoadout {
    int   basicAssetId      = -1;
    int   slotAssetId[3]    = { -1, -1, -1 };
    float slotCost[3]       = { 0.f, 0.f, 0.f };
    float slotCooldownMs[3] = { 0.f, 0.f, 0.f };
};

struct SkillLoadout {
    std::array<WeaponLoadout, 4> byWeapon{};

    static SkillLoadout build(const std::vector<SkillAsset>& assets) {
        SkillLoadout out{};
        for (const SkillAsset& a : assets) {
            if (a.weaponType > 3) continue;
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

    // charge cost for a (weapon, slot); 0 if out of range (avoids div-by-zero).
    float cost(unsigned weaponOrdinal, int slot) const {
        if (weaponOrdinal >= 4 || slot < 0 || slot >= 3) return 0.f;
        return byWeapon[weaponOrdinal].slotCost[slot];
    }
};

#endif  // __rs_skill_skillLoadout_HPP
