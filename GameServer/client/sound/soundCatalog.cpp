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
	{ "lobby",     "../resources/audio/bgm/lobby.wav",   Bus::Bgm, /*loop*/true,  /*stream*/true,  1.0f },
	{ "ingame",    "../resources/audio/bgm/Action 5 (Loop).wav",  Bus::Bgm, /*loop*/true,  /*stream*/true,  1.0f },

	// --- UI ---
	{ "ui_click",  "../resources/audio/sfx/ui_click.wav", Bus::Ui,  false, false, 1.0f },

	// --- Skill SFX (positional, fired by PlaySound timeline events) ---
	{ "sword_slash_1",      "../resources/audio/sfx/sword/sword_slash_1.mp3",      Bus::Sfx, false, false, 1.0f },
	{ "sword_slash_finish", "../resources/audio/sfx/sword/sword_slash_finish.mp3", Bus::Sfx, false, false, 1.0f },
	{ "sword_slash_7",      "../resources/audio/sfx/sword/sword_slash_7.mp3",      Bus::Sfx, false, false, 1.0f },
	{ "slash_wave",         "../resources/audio/sfx/sword/slash_wave.mp3",         Bus::Sfx, false, false, 1.0f },

	// --- Bow skill SFX (positional; timeline PlaySound + particle-spawn hooks) ---
	{ "arrow_default",     "../resources/audio/sfx/bow/arrow_default.mp3",     Bus::Sfx, false, false, 1.0f },
	{ "arrow_rain",        "../resources/audio/sfx/bow/arrow_rain.mp3",        Bus::Sfx, false, false, 1.0f },
	{ "arrow_charge",      "../resources/audio/sfx/bow/arrow_charge.mp3",      Bus::Sfx, false, false, 1.0f },
	{ "charge_shoot",      "../resources/audio/sfx/bow/charge_shoot.mp3",      Bus::Sfx, false, false, 1.0f },
	{ "charge_explosion",  "../resources/audio/sfx/bow/charge_explosion.mp3",  Bus::Sfx, false, false, 1.0f },
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

std::span<const CatalogEntry> allSounds() {
	return { kCatalog.data(), kCatalog.size() };
}

}	// namespace snd
