#include "pch.hpp"
#include "soundManager.hpp"
#include "soundCatalog.hpp"

// miniaudio.h is included here for declarations only. The implementation is
// compiled in miniaudio_impl.cpp (the only TU with MINIAUDIO_IMPLEMENTATION).
#include "miniaudio.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace {

// Max number of one-shot SFX that can sound simultaneously. The pool naturally
// caps overlap (e.g. when many enemies are hit on the same frame).
constexpr int kSfxVoiceCount = 32;

constexpr int kBusCount = static_cast<int>(SoundManager::Bus::Count);

// Minimum gap between two plays of the *same* SFX. Collapses bursts of identical
// sounds (e.g. dozens of hits on one frame) that would otherwise clip and stack.
constexpr float kSfxDedupeCooldownSec = 0.035f;

inline float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

}	// namespace

struct SoundManager::Impl {
	bool enabled = false;

	ma_engine      engine{};
	ma_sound_group buses[kBusCount]{};	// indexed by Bus

	// BGM: two slots so play/stop can crossfade. curBgm is the foreground slot.
	struct BgmSlot {
		ma_sound    sound{};
		bool        active = false;
		std::string name;
	};
	BgmSlot bgm[2]{};
	int     curBgm = 0;

	// One-shot SFX voice pool.
	struct Voice {
		ma_sound sound{};
		bool     inUse = false;
	};
	Voice voices[kSfxVoiceCount]{};

	float masterVol = 1.0f;
	float busVol[kBusCount] = { 1.0f, 1.0f, 1.0f };

	// Monotonic clock (seconds) advanced by update(); used for SFX de-duplication.
	float clockSec = 0.0f;
	std::unordered_map<std::string, float> lastSfxTime;

	// De-spammed warnings for missing/failed files.
	std::unordered_set<std::string> warned;

	// Returns true if this SFX may play now, throttling identical-name bursts.
	bool sfxAllowed(std::string_view name) {
		std::string key(name);
		auto it = lastSfxTime.find(key);
		if (it != lastSfxTime.end() && (clockSec - it->second) < kSfxDedupeCooldownSec) {
			return false;
		}
		lastSfxTime[key] = clockSec;
		return true;
	}

	void warnOnce(std::string_view path) {
		std::string key(path);
		if (warned.insert(key).second) {
			gSharedLog << "[Sound] Warning: failed to load '" << key << "' (missing or unsupported)\n";
		}
	}

	// Starts a one-shot voice from the pool. Returns false if the pool is full
	// (the sound is simply dropped) or the file failed to load.
	bool startOneShot(const snd::CatalogEntry& e, bool spatial, float volume,
	                  bool hasPos, float px, float py, float pz) {
		int idx = -1;
		for (int i = 0; i < kSfxVoiceCount; ++i) {
			if (!voices[i].inUse) { idx = i; break; }
		}
		if (idx < 0) return false;

		Voice& v = voices[idx];
		ma_uint32 flags = MA_SOUND_FLAG_DECODE;
		if (!spatial) flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;

		std::string path(e.path);
		if (ma_sound_init_from_file(&engine, path.c_str(), flags,
		                            &buses[static_cast<int>(e.bus)], nullptr, &v.sound) != MA_SUCCESS) {
			warnOnce(e.path);
			return false;
		}
		ma_sound_set_volume(&v.sound, e.defaultVolume * clamp01(volume));
		if (spatial && hasPos) {
			ma_sound_set_position(&v.sound, px, py, pz);
		}
		ma_sound_start(&v.sound);
		v.inUse = true;
		return true;
	}
};

SoundManager::SoundManager() : impl_(std::make_unique<Impl>()) {}
SoundManager::~SoundManager() { shutdown(); }

bool SoundManager::init() {
	if (impl_->enabled) return true;

	if (ma_engine_init(nullptr, &impl_->engine) != MA_SUCCESS) {
		gSharedLog << "[Sound] No audio device available; sound disabled (all calls are no-ops).\n";
		impl_->enabled = false;
		return false;
	}

	for (int i = 0; i < kBusCount; ++i) {
		if (ma_sound_group_init(&impl_->engine, 0, nullptr, &impl_->buses[i]) != MA_SUCCESS) {
			gSharedLog << "[Sound] Failed to create mixer bus " << i << "; sound disabled.\n";
			// Unwind already-created buses and the engine.
			for (int j = 0; j < i; ++j) ma_sound_group_uninit(&impl_->buses[j]);
			ma_engine_uninit(&impl_->engine);
			impl_->enabled = false;
			return false;
		}
		ma_sound_group_set_volume(&impl_->buses[i], impl_->busVol[i]);
	}

	ma_engine_set_volume(&impl_->engine, impl_->masterVol);
	impl_->enabled = true;
	gSharedLog << "[Sound] Audio engine initialized.\n";
	return true;
}

void SoundManager::shutdown() {
	if (!impl_ || !impl_->enabled) return;

	for (auto& v : impl_->voices) {
		if (v.inUse) { ma_sound_uninit(&v.sound); v.inUse = false; }
	}
	for (auto& b : impl_->bgm) {
		if (b.active) { ma_sound_uninit(&b.sound); b.active = false; b.name.clear(); }
	}
	for (int i = 0; i < kBusCount; ++i) {
		ma_sound_group_uninit(&impl_->buses[i]);
	}
	ma_engine_uninit(&impl_->engine);
	impl_->enabled = false;
}

bool SoundManager::enabled() const { return impl_->enabled; }

void SoundManager::update(float deltaSeconds) {
	if (!impl_->enabled) return;

	impl_->clockSec += deltaSeconds;

	// Reclaim finished one-shot voices.
	for (auto& v : impl_->voices) {
		if (v.inUse && ma_sound_at_end(&v.sound)) {
			ma_sound_uninit(&v.sound);
			v.inUse = false;
		}
	}
	// Reclaim any BGM slot whose (possibly faded-out) playback has stopped. A
	// looping foreground track keeps playing and is not reclaimed.
	for (auto& b : impl_->bgm) {
		if (b.active && !ma_sound_is_playing(&b.sound)) {
			ma_sound_uninit(&b.sound);
			b.active = false;
			b.name.clear();
		}
	}
}

void SoundManager::playBgm(std::string_view name, float fadeMs, bool loop) {
	if (!impl_->enabled) return;

	const snd::CatalogEntry* e = snd::findSound(name);
	if (!e) { impl_->warnOnce(name); return; }

	// Already playing this track in the foreground: nothing to do.
	if (impl_->bgm[impl_->curBgm].active && impl_->bgm[impl_->curBgm].name == name) return;

	const int next = 1 - impl_->curBgm;
	// If a previous crossfade has not finished, hard-stop the stale slot.
	if (impl_->bgm[next].active) {
		ma_sound_uninit(&impl_->bgm[next].sound);
		impl_->bgm[next].active = false;
		impl_->bgm[next].name.clear();
	}

	const ma_uint32 flags = MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION;
	std::string path(e->path);
	if (ma_sound_init_from_file(&impl_->engine, path.c_str(), flags,
	                            &impl_->buses[static_cast<int>(Bus::Bgm)], nullptr,
	                            &impl_->bgm[next].sound) != MA_SUCCESS) {
		impl_->warnOnce(e->path);
		return;
	}
	ma_sound_set_looping(&impl_->bgm[next].sound, loop ? MA_TRUE : MA_FALSE);
	impl_->bgm[next].active = true;
	impl_->bgm[next].name.assign(name);

	if (fadeMs > 0.0f) {
		ma_sound_set_fade_in_milliseconds(&impl_->bgm[next].sound, 0.0f, e->defaultVolume,
		                                  static_cast<ma_uint64>(fadeMs));
	} else {
		ma_sound_set_volume(&impl_->bgm[next].sound, e->defaultVolume);
	}
	ma_sound_start(&impl_->bgm[next].sound);

	// Fade out the previous foreground track; it is reclaimed in update().
	if (impl_->bgm[impl_->curBgm].active) {
		if (fadeMs > 0.0f) {
			ma_sound_stop_with_fade_in_milliseconds(&impl_->bgm[impl_->curBgm].sound,
			                                        static_cast<ma_uint64>(fadeMs));
		} else {
			ma_sound_stop(&impl_->bgm[impl_->curBgm].sound);
		}
	}
	impl_->curBgm = next;
}

void SoundManager::stopBgm(float fadeMs) {
	if (!impl_->enabled) return;
	auto& fg = impl_->bgm[impl_->curBgm];
	if (!fg.active) return;
	if (fadeMs > 0.0f) {
		ma_sound_stop_with_fade_in_milliseconds(&fg.sound, static_cast<ma_uint64>(fadeMs));
	} else {
		ma_sound_stop(&fg.sound);
	}
	// Slot is reclaimed in update() once playback has actually stopped.
}

void SoundManager::playSfx(std::string_view name, float volume) {
	if (!impl_->enabled) return;
	const snd::CatalogEntry* e = snd::findSound(name);
	if (!e) { impl_->warnOnce(name); return; }
	if (!impl_->sfxAllowed(name)) return;
	impl_->startOneShot(*e, /*spatial*/false, volume, /*hasPos*/false, 0, 0, 0);
}

void MU_CALLCONV SoundManager::playSfx3D(std::string_view name, mu::Vec3 worldPos, float volume) {
	if (!impl_->enabled) return;
	const snd::CatalogEntry* e = snd::findSound(name);
	if (!e) { impl_->warnOnce(name); return; }
	if (!impl_->sfxAllowed(name)) return;
	impl_->startOneShot(*e, /*spatial*/true, volume, /*hasPos*/true,
	                    worldPos.x(), worldPos.y(), worldPos.z());
}

void MU_CALLCONV SoundManager::setListener(mu::Vec3 pos, mu::Vec3 forward, mu::Vec3 up) {
	if (!impl_->enabled) return;
	// NOTE: the game uses a left-handed coordinate system (DirectX). miniaudio's
	// default panning is right-handed, so left/right may need an axis flip; this
	// is tuned in Stage 3 when 3D SFX are wired to gameplay.
	ma_engine_listener_set_position(&impl_->engine, 0, pos.x(), pos.y(), pos.z());
	ma_engine_listener_set_direction(&impl_->engine, 0, forward.x(), forward.y(), forward.z());
	ma_engine_listener_set_world_up(&impl_->engine, 0, up.x(), up.y(), up.z());
}

void SoundManager::setMasterVolume(float v) {
	impl_->masterVol = clamp01(v);
	if (impl_->enabled) ma_engine_set_volume(&impl_->engine, impl_->masterVol);
}

void SoundManager::setBusVolume(Bus bus, float v) {
	const int i = static_cast<int>(bus);
	if (i < 0 || i >= kBusCount) return;
	impl_->busVol[i] = clamp01(v);
	if (impl_->enabled) ma_sound_group_set_volume(&impl_->buses[i], impl_->busVol[i]);
}

float SoundManager::masterVolume() const { return impl_->masterVol; }

float SoundManager::busVolume(Bus bus) const {
	const int i = static_cast<int>(bus);
	if (i < 0 || i >= kBusCount) return 0.0f;
	return impl_->busVol[i];
}
