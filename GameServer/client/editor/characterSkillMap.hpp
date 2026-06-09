#ifndef __editor_characterSkillMap_HPP
#define __editor_characterSkillMap_HPP

// Global, compile-time mapping of test characters to the skills they own.
//
// The skill editor classifies skills by character first: the character dropdown
// selects a caster, and the skill dropdown then lists that character's skills.
// Skill names here must match SkillAsset::name (compiled from resources/skills/*.lua);
// the skill dropdown only shows names that actually exist in the SkillSystem registry.
//
// Player and Goblin are wired today. Additional monsters (Anubis, Fishman, ...)
// can be appended once their models/skeletons are loaded by AssetManager and a
// spawn branch is added; the mapping structure already supports them.

#include <string_view>
#include <vector>

namespace Editor {

enum class CharacterKind {
    Player,
    Goblin,
};

struct CharacterDef {
    CharacterKind                 kind;
    std::string_view              label;   // shown in the character dropdown
    std::vector<std::string_view> skills;  // SkillAsset names this character may cast
};

// Player skills are paired 1:1 with the legacy effect dropdown (one skill per VFX);
// each name matches a resources/skills/*.lua SkillAsset and a vfxId bound in
// StandAlone::Game (skillVfxById_). Order mirrors the old effect dropdown.
inline const std::vector<CharacterDef> kCharacterSkillMap = {
    { CharacterKind::Player, "Player", {
        "SwordSlash",            // Slash 1
        "SlashWave",
        "SlashCombo",
        "Slash7",
        "Spikes",
        "CrystalsFrontAttack",
        "AoESlashGreen",
        "RedEnergyExplosion",
        "CrystalsCrossFade",
        "Arrow",
        "ArrowVolley",
        "ArrowRain",
        "EnergyExplosionArrow",
        "TornadoShot",
        "Piercing",
        "PiercingSlash",
        "PiercingCircleSlash",
        "PiercingMulti",
    } },
    { CharacterKind::Goblin, "Goblin", {} },
};

}   // namespace Editor

#endif  // __editor_characterSkillMap_HPP
