// Single translation unit that compiles the miniaudio implementation.
//
// IMPORTANT: this file is built WITHOUT the precompiled header (set
// PrecompiledHeader=NotUsing for this file in client.vcxproj), exactly like the
// vendored d3dx12 sources. miniaudio.h is a vendored external dependency -- do
// not modify it.
//
// On Windows miniaudio dynamically loads its backend (WASAPI) at runtime, so no
// extra .lib needs to be linked and no DLL needs to be shipped.

#define MINIAUDIO_IMPLEMENTATION

// We only need playback; trim capture/enumeration paths we never use to keep the
// implementation lean. Decoders for WAV/MP3/FLAC stay enabled (BGM may be MP3 or
// FLAC, SFX is WAV).
#define MA_NO_ENCODING

// Silence warnings originating from the vendored single-header implementation.
#pragma warning(push, 0)
#include "miniaudio.h"
#pragma warning(pop)
