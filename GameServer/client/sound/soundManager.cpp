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
#include <vector>

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
		bool        pendingStart = false;
		std::string name;
	};
	BgmSlot bgm[2]{};
	int     curBgm = 0;

	// One-shot SFX voice pool.
	struct Voice {
		ma_sound sound{};
		bool     inUse = false;
		float    stopAtSec  = -1.f;  // clockSec at which to fade-stop this voice; <0 = play to end
		float    stopFadeMs = 0.f;   // fade length used when the scheduled stop fires
	};
	Voice voices[kSfxVoiceCount]{};

	float masterVol = 1.0f;
	float busVol[kBusCount] = { 1.0f, 1.0f, 1.0f };

	// Monotonic clock (seconds) advanced by update(); used for SFX de-duplication.
	float clockSec = 0.0f;
	std::unordered_map<std::string, float> lastSfxTime;

	// De-spammed warnings for missing/failed files.
	std::unordered_set<std::string> warned;

	// Pre-decoded SFX sources, keyed by file path. Each is loaded once at init
	// (MA_SOUND_FLAG_DECODE) and kept alive for the manager's lifetime so the
	// resource manager caches the decoded PCM; startOneShot then clones a voice
	// with ma_sound_init_copy() -- no disk read or decode on the play path.
	// Without this the FIRST play of a sound decodes synchronously on the game
	// thread, delaying its onset (the first combat hit lags behind its visual).
	// unordered_map nodes are address-stable, which ma_sound (a node-graph node)
	// requires, so the sounds are initialised in place via operator[].
	std::unordered_map<std::string, ma_sound> sfxTemplates;

	// Leading-silence offset (PCM frames) to skip per preloaded sound, measured
	// once at preload. mp3 files carry encoder-delay padding (and a clip may have a
	// quiet head) that decodes into leading silence; skipping it tightens the onset
	// so the hit lands on time. Keyed by file path; absent/0 = skip nothing.
	std::unordered_map<std::string, ma_uint64> sfxLeadFrames;

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

	// Returns the count of leading PCM frames at/below the silence floor in a
	// fully-decoded sound (mp3 encoder-delay padding + any quiet clip head). The
	// sound's read cursor is restored to 0 afterwards. 0 = nothing to skip.
	ma_uint64 detectLeadSilenceFrames(ma_sound& s) {
		ma_format fmt = ma_format_unknown;
		ma_uint32 ch  = 0;
		if (ma_sound_get_data_format(&s, &fmt, &ch, nullptr, nullptr, 0) != MA_SUCCESS || ch == 0)
			return 0;
		ma_data_source* ds = ma_sound_get_data_source(&s);
		if (!ds) return 0;

		constexpr ma_uint64 kChunk = 4096;
		ma_uint64 lead = 0;
		bool found = false;

		if (fmt == ma_format_f32) {
			std::vector<float> buf(kChunk * ch);
			for (;;) {
				ma_uint64 read = 0;
				if (ma_data_source_read_pcm_frames(ds, buf.data(), kChunk, &read) != MA_SUCCESS || read == 0) break;
				for (ma_uint64 f = 0; f < read && !found; ++f) {
					for (ma_uint32 c = 0; c < ch; ++c) {
						float a = buf[f * ch + c];
						if (a < 0.f) a = -a;
						if (a > 0.003f) { lead += f; found = true; break; }
					}
				}
				if (found) break;
				lead += read;
			}
		} else if (fmt == ma_format_s16) {
			std::vector<ma_int16> buf(kChunk * ch);
			for (;;) {
				ma_uint64 read = 0;
				if (ma_data_source_read_pcm_frames(ds, buf.data(), kChunk, &read) != MA_SUCCESS || read == 0) break;
				for (ma_uint64 f = 0; f < read && !found; ++f) {
					for (ma_uint32 c = 0; c < ch; ++c) {
						int a = buf[f * ch + c];
						if (a < 0) a = -a;
						if (a > 100) { lead += f; found = true; break; }
					}
				}
				if (found) break;
				lead += read;
			}
		}

		ma_data_source_seek_to_pcm_frame(ds, 0);   // restore cursor for cloning
		return found ? lead : 0;
	}

	// Pre-decode every non-streamed catalog sound so the first in-game play is
	// instant (no synchronous decode on the game thread). Missing/failed files
	// are skipped silently; startOneShot then falls back to init-from-file, which
	// warns once. Streamed/BGM entries are left to the streaming path.
	void preloadSfx() {
		int loaded = 0, failed = 0;
		for (const snd::CatalogEntry& e : snd::allSounds()) {
			if (e.stream || e.bus == SoundManager::Bus::Bgm) continue;
			std::string key(e.path);
			if (sfxTemplates.find(key) != sfxTemplates.end()) continue;
			ma_sound& slot = sfxTemplates[key];   // node-stable storage; init in place
			const ma_uint32 flags = MA_SOUND_FLAG_DECODE
			                      | MA_SOUND_FLAG_NO_SPATIALIZATION
			                      | MA_SOUND_FLAG_NO_DEFAULT_ATTACHMENT;
			if (ma_sound_init_from_file(&engine, key.c_str(), flags, nullptr, nullptr, &slot) == MA_SUCCESS) {
				++loaded;
				const ma_uint64 lead = detectLeadSilenceFrames(slot);
				if (lead > 0) sfxLeadFrames[key] = lead;
			} else {
				sfxTemplates.erase(key);   // never decoded; drop the empty slot
				++failed;
				gSharedLog << "[Sound] SFX preload FAILED: '" << key << "'\n";
			}
		}
		gSharedLog << "[Sound] SFX preloaded: " << loaded << " ok, " << failed << " failed.\n";

		// Warm the full play path (voice clone + mixer + device) once per sound at
		// zero volume, so the FIRST audible play in-game pays no cold-start latency
		// (first voice activation / first resource-manager data-source copy). Silent.
		for (const snd::CatalogEntry& e : snd::allSounds()) {
			if (e.stream || e.bus == SoundManager::Bus::Bgm) continue;
			if (sfxTemplates.find(std::string(e.path)) == sfxTemplates.end()) continue;
			startOneShot(e, /*spatial*/true, /*volume*/0.f, /*hasPos*/false, 0.f, 0.f, 0.f);
		}
	}

	// Starts a one-shot voice from the pool. Returns false if the pool is full
	// (the sound is simply dropped) or the file failed to load.
	bool startOneShot(const snd::CatalogEntry& e, bool spatial, float volume,
	                  bool hasPos, float px, float py, float pz,
	                  float maxDurationMs = 0.f, float fadeMs = 0.f) {
		int idx = -1;
		for (int i = 0; i < kSfxVoiceCount; ++i) {
			if (!voices[i].inUse) { idx = i; break; }
		}
		if (idx < 0) return false;

		Voice& v = voices[idx];
		const std::string pathKey(e.path);

		// Prefer a pre-decoded template (clone shares the cached PCM, no disk read
		// or decode); fall back to loading from file if this sound was not preloaded.
		bool ok = false;
		if (auto it = sfxTemplates.find(pathKey); it != sfxTemplates.end()) {
			const ma_uint32 flags = spatial ? 0u : MA_SOUND_FLAG_NO_SPATIALIZATION;
			ok = (ma_sound_init_copy(&engine, &it->second, flags,
			                         &buses[static_cast<int>(e.bus)], &v.sound) == MA_SUCCESS);
		} else {
			ma_uint32 flags = MA_SOUND_FLAG_DECODE;
			if (!spatial) flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;
			ok = (ma_sound_init_from_file(&engine, pathKey.c_str(), flags,
			                              &buses[static_cast<int>(e.bus)], nullptr, &v.sound) == MA_SUCCESS);
		}
		if (!ok) {
			warnOnce(e.path);
			return false;
		}

		// Skip leading silence baked into the decoded PCM (mp3 encoder-delay padding
		// / quiet clip head), measured once at preload, so the onset is tight.
		if (auto lit = sfxLeadFrames.find(pathKey); lit != sfxLeadFrames.end() && lit->second > 0) {
			ma_sound_seek_to_pcm_frame(&v.sound, lit->second);
		}
		ma_sound_set_volume(&v.sound, e.defaultVolume * clamp01(volume));
		if (spatial && hasPos) {
			ma_sound_set_position(&v.sound, px, py, pz);
		}
		ma_sound_start(&v.sound);
		v.inUse = true;
		// Schedule a fade-out so a long file does not outlast a short effect.
		v.stopAtSec  = (maxDurationMs > 0.f) ? (clockSec + maxDurationMs * 0.001f) : -1.f;
		v.stopFadeMs = fadeMs;
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

	// Warm the SFX cache so the first play of each sound is instant (no decode hitch).
	impl_->preloadSfx();

	gSharedLog << "[Sound] Audio engine initialized.\n";
	return true;
}

void SoundManager::shutdown() {
	if (!impl_ || !impl_->enabled) return;

	for (auto& v : impl_->voices) {
		if (v.inUse) { ma_sound_uninit(&v.sound); v.inUse = false; }
	}
	for (auto& b : impl_->bgm) {
		if (b.active) {
			ma_sound_uninit(&b.sound);
			b.active = false;
			b.pendingStart = false;
			b.name.clear();
		}
	}
	for (auto& [path, tmpl] : impl_->sfxTemplates) {
		ma_sound_uninit(&tmpl);
	}
	impl_->sfxTemplates.clear();
	impl_->sfxLeadFrames.clear();
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

	// Fire scheduled fade-stops (sounds cut to match a finite effect duration).
	for (auto& v : impl_->voices) {
		if (v.inUse && v.stopAtSec >= 0.f && impl_->clockSec >= v.stopAtSec) {
			if (v.stopFadeMs > 0.f)
				ma_sound_stop_with_fade_in_milliseconds(&v.sound, static_cast<ma_uint64>(v.stopFadeMs));
			else
				ma_sound_stop(&v.sound);
			v.stopAtSec = -1.f;   // fired once; the voice is reclaimed below when it actually stops
		}
	}
	// Reclaim voices that have finished (natural end) or whose scheduled fade-stop completed.
	for (auto& v : impl_->voices) {
		if (v.inUse && !ma_sound_is_playing(&v.sound)) {
			ma_sound_uninit(&v.sound);
			v.inUse = false;
			v.stopAtSec = -1.f;
		}
	}
	// Reclaim any BGM slot whose (possibly faded-out) playback has stopped. A
	// looping foreground track keeps playing and is not reclaimed. A delayed
	// start is not considered finished while it is waiting for its start time.
	for (auto& b : impl_->bgm) {
		if (!b.active) continue;
		if (b.pendingStart) {
			if (ma_sound_is_playing(&b.sound)) b.pendingStart = false;
			else continue;
		}
		if (!ma_sound_is_playing(&b.sound)) {
			ma_sound_uninit(&b.sound);
			b.active = false;
			b.pendingStart = false;
			b.name.clear();
		}
	}
}

void SoundManager::playBgm(std::string_view name, float fadeMs, bool loop) {
	playBgm(name, fadeMs, fadeMs, 0.f, loop);
}

void SoundManager::playBgm(std::string_view name, float fadeOutMs, float fadeInMs,
	                       float fadeInDelayMs, bool loop) {
	if (!impl_->enabled) return;

	const snd::CatalogEntry* e = snd::findSound(name);
	if (!e) { impl_->warnOnce(name); return; }

	fadeOutMs     = std::max(fadeOutMs, 0.f);
	fadeInMs      = std::max(fadeInMs, 0.f);
	fadeInDelayMs = std::max(fadeInDelayMs, 0.f);

	// Already playing this track in the foreground: nothing to do.
	if (impl_->bgm[impl_->curBgm].active && impl_->bgm[impl_->curBgm].name == name) return;

	const int next = 1 - impl_->curBgm;
	// If a previous crossfade has not finished, hard-stop the stale slot.
	if (impl_->bgm[next].active) {
		ma_sound_uninit(&impl_->bgm[next].sound);
		impl_->bgm[next].active = false;
		impl_->bgm[next].pendingStart = false;
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
	impl_->bgm[next].pendingStart = fadeInDelayMs > 0.f;
	impl_->bgm[next].name.assign(name);

	ma_uint64 startTimeMs = 0;
	if (fadeInDelayMs > 0.f) {
		startTimeMs = ma_engine_get_time_in_milliseconds(&impl_->engine)
		            + static_cast<ma_uint64>(fadeInDelayMs);
		ma_sound_set_start_time_in_milliseconds(&impl_->bgm[next].sound, startTimeMs);
	}

	if (fadeInMs > 0.f) {
		if (fadeInDelayMs > 0.f) {
			ma_sound_set_fade_start_in_milliseconds(&impl_->bgm[next].sound,
			                                        0.f, e->defaultVolume,
			                                        static_cast<ma_uint64>(fadeInMs),
			                                        startTimeMs);
		} else {
			ma_sound_set_fade_in_milliseconds(&impl_->bgm[next].sound, 0.f, e->defaultVolume,
			                                  static_cast<ma_uint64>(fadeInMs));
		}
	} else {
		ma_sound_set_volume(&impl_->bgm[next].sound, e->defaultVolume);
	}
	ma_sound_start(&impl_->bgm[next].sound);

	// Fade out the previous foreground track; it is reclaimed in update().
	if (impl_->bgm[impl_->curBgm].active) {
		// A transition superseded before its delayed track started can be reclaimed
		// immediately instead of remaining protected as a pending start forever.
		impl_->bgm[impl_->curBgm].pendingStart = false;
		if (fadeOutMs > 0.f) {
			ma_sound_stop_with_fade_in_milliseconds(&impl_->bgm[impl_->curBgm].sound,
			                                        static_cast<ma_uint64>(fadeOutMs));
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

void MU_CALLCONV SoundManager::playSfx3D(std::string_view name, mu::Vec3 worldPos, float volume,
                                         float maxDurationMs, float fadeMs) {
	if (!impl_->enabled) return;
	const snd::CatalogEntry* e = snd::findSound(name);
	if (!e) { impl_->warnOnce(name); return; }
	if (!impl_->sfxAllowed(name)) return;
	impl_->startOneShot(*e, /*spatial*/true, volume, /*hasPos*/true,
	                    worldPos.x(), worldPos.y(), worldPos.z(), maxDurationMs, fadeMs);
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
