#include "pch.hpp"
#include "soundCatalog.hpp"

#include <array>

namespace snd {

namespace {

using Bus = SoundManager::Bus;

// Logical name -> file. Paths are exe-relative, mirroring resources/ layout.
// These are placeholders: drop matching files under resources/audio/ and the
// system picks them up. Missing files just warn at play time (no crash).
constexpr std::array kCatalog = std::to_array<CatalogEntry>({
	// --- Background music (streamed, looped) ---
	{ "lobby",     "../resources/audio/bgm/lobby.mp3",   Bus::Bgm, /*loop*/true,  /*stream*/true,  1.0f },
	{ "ingame",    "../resources/audio/bgm/ingame.wav",  Bus::Bgm, /*loop*/true,  /*stream*/true,  1.0f },

	// --- UI ---
	{ "ui_click",  "../resources/audio/sfx/ui_click.wav", Bus::Ui,  false, false, 1.0f },
	{ "ui_hover",  "../resources/audio/sfx/ui_hover.wav", Bus::Ui,  false, false, 0.7f },

	// --- Combat SFX (positional) ---
	{ "hit",       "../resources/audio/sfx/hit.wav",       Bus::Sfx, false, false, 1.0f },
	{ "death",     "../resources/audio/sfx/death.wav",     Bus::Sfx, false, false, 1.0f },
	{ "attack",    "../resources/audio/sfx/attack.wav",    Bus::Sfx, false, false, 0.9f },
	{ "skill_hit", "../resources/audio/sfx/skill_hit.wav", Bus::Sfx, false, false, 1.0f },

	// --- HUD ---
	// Played when a skill slot reaches its first usable stack (0 -> 1).
	{ "skill_ready", "../resources/audio/sfx/skill_ready.wav", Bus::Ui, false, false, 1.0f },
});

}	// namespace

const CatalogEntry* findSound(std::string_view name) {
	for (const auto& e : kCatalog) {
		if (e.name == name) {
			return &e;
		}
	}
	return nullptr;
}

}	// namespace snd
