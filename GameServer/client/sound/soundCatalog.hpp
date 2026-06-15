#ifndef __soundCatalog_HPP
#define __soundCatalog_HPP

// Sound catalog: maps logical sound names to a file path and playback
// properties. Kept as a compile-time C++ table (zero dependency, type-safe,
// no parse cost). It can later be migrated to a JSON catalog for non-recompile
// tuning, consistent with the particle JSON pipeline.
//
// New file: English-only comments to stay cp949-safe (see client/CLAUDE.md).

#include <string_view>
#include "soundManager.hpp"   // SoundManager::Bus

namespace snd {

struct CatalogEntry {
	std::string_view name;          // logical key used by game code
	std::string_view path;          // exe-relative file path
	SoundManager::Bus bus;          // mixer bus this sound routes to
	bool  loop;                     // loop on play (BGM)
	bool  stream;                   // stream from disk (BGM) vs decode to memory (SFX)
	float defaultVolume;            // base volume (0..1)
};

// Returns the entry for `name`, or nullptr if not found.
const CatalogEntry* findSound(std::string_view name);

}	// namespace snd

#endif	// __soundCatalog_HPP
