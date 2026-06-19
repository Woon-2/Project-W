#ifndef __soundManager_HPP
#define __soundManager_HPP

// Sound system: a thin engine-side abstraction over the audio backend.
//
// The backend (miniaudio) is fully hidden behind a pimpl so that miniaudio.h is
// compiled in only two translation units (soundManager.cpp and
// miniaudio_impl.cpp). Game code talks only to this interface, which keeps the
// backend swappable (e.g. to FMOD) without touching game logic.
//
// Threading: every public method must be called from the game thread only
// (the update/render thread). The backend runs its own internal audio thread;
// do NOT call this from IOCP/network worker threads. Network handlers should
// post events and let the game-thread dispatch loop trigger sounds.
//
// New file: English-only comments to stay cp949-safe (see client/CLAUDE.md).

#include <memory>
#include <string_view>
#include "mathUtil.hpp"   // mu::Vec3

class SoundManager {
public:
	// Logical mixer buses. Each maps to a backend sound group under the master.
	enum class Bus : unsigned { Bgm, Sfx, Ui, Count };

	SoundManager();
	~SoundManager();

	SoundManager(const SoundManager&) = delete;
	SoundManager& operator=(const SoundManager&) = delete;

	// Opens the audio device and builds the bus graph. On failure (e.g. no audio
	// device) it logs once and leaves the manager in a disabled state where every
	// other call is a safe no-op. Returns true on success.
	bool init();
	void shutdown();
	bool enabled() const;

	// Per-frame tick on the game thread: reclaims finished one-shot voices and
	// the faded-out BGM slot. deltaSeconds is the frame delta in seconds.
	void update(float deltaSeconds);

	// ---- Background music (single logical track, streamed from disk) ----
	// Crossfades from the current track to the catalog entry `name`. Re-requesting
	// the already-playing track is a no-op. fadeMs <= 0 switches instantly.
	void playBgm(std::string_view name, float fadeMs = 800.f, bool loop = true);
	void stopBgm(float fadeMs = 600.f);

	// ---- Sound effects (one-shot, fire-and-forget from a fixed voice pool) ----
	void playSfx(std::string_view name, float volume = 1.f);                 // 2D
	// maxDurationMs > 0 schedules a fade-out (over fadeMs) that many ms after the
	// voice starts, so a one-shot can be cut to match a short effect; 0 = play full.
	void MU_CALLCONV playSfx3D(std::string_view name, mu::Vec3 worldPos,     // positional
	                           float volume = 1.f,
	                           float maxDurationMs = 0.f, float fadeMs = 0.f);

	// ---- 3D listener: set once per frame from the camera before update() ----
	void MU_CALLCONV setListener(mu::Vec3 pos, mu::Vec3 forward, mu::Vec3 up);

	// ---- Volume control (0..1), persists across tracks/voices ----
	void  setMasterVolume(float v);
	void  setBusVolume(Bus bus, float v);
	float masterVolume() const;
	float busVolume(Bus bus) const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

#endif	// __soundManager_HPP
