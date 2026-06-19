#ifndef __editor_characterSkillMap_HPP
#define __editor_characterSkillMap_HPP

// Global, compile-time mapping of test characters to the skills they own.
//
// The skill editor classifies skills by character first: the character dropdown
// selects a caster, and the skill dropdown then lists that character's skills.
// Skill names here must match SkillAsset::name (compiled from resources/skills/*.lua);
// the skill dropdown only shows names that actually exist in the SkillSystem registry.
//
// Player is wired today. Monster casters (Goblin, Mushroom, Snake, Birdy, Bomber,
// Slime, Treant) are scaffolded but their kCharacterSkillMap entries are disabled
// behind `#if 0` guards (see below). Enabling a monster requires uncommenting the
// matching [Name] guards across object.*, AssetManager.*, editorController.cpp and
// adding its model/anim resources. The CharacterKind enum values stay defined so
// flipping a single entry's guard compiles cleanly.

#include <string_view>
#include <vector>

namespace Editor {

enum class CharacterKind {
    Player,
    Goblin,
    Mushroom,
    Snake,
    Birdy,
    Bomber,
    Slime,
    Treant,
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
    // ---- Monster skill casters (disabled scaffolding) ----------------------
    // Each entry's skills are the monster's attack lua names (resources/skills/
    // *_attackN.lua -> SkillAsset::name). lua attackIndex selects the clip.
    // Enable a monster by flipping ITS [Name] guard here AND in object.*,
    // AssetManager.*, editorController.cpp (setMonsterCaster), then provide the
    // model/anim resources. Resources already present: Goblin, Mushroom, Snake,
    // Bomber. Missing (must be added): Birdy, Slime, Treant.
#if 0  // [Goblin]
    { CharacterKind::Goblin,   "Goblin",   { "Goblin_Attack1", "Goblin_Attack2", "Goblin_Attack3" } },
#endif
#if 0  // [Mushroom]
    { CharacterKind::Mushroom, "Mushroom", { "Mushroom_Attack1", "Mushroom_Attack2" } },
#endif
#if 0  // [Snake]
    { CharacterKind::Snake,    "Snake",    { "Snake_Attack1" } },
#endif
#if 0  // [Birdy]
    { CharacterKind::Birdy,    "Birdy",    { "Birdy_Attack1", "Birdy_Attack2" } },
#endif
#if 0  // [Bomber]
    { CharacterKind::Bomber,   "Bomber",   { "Bomber_Attack1" } },
#endif
#if 0  // [Slime]
    { CharacterKind::Slime,    "Slime",    { "Slime_Attack1" } },
#endif
#if 0  // [Treant]
    { CharacterKind::Treant,   "Treant",   { "Treant_SpinKick", "Treant_Clap", "Treant_Punch" } },
#endif
};

}   // namespace Editor

#endif  // __editor_characterSkillMap_HPP
