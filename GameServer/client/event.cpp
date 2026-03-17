#include "pch.hpp"
#include "event.hpp"

namespace detail {

Pool<RawChunk<4>> gPool4{32u};
Pool<RawChunk<16>> gPool16{32u};

}

NullEventBus gNullEventBus{};