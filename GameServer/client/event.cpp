#include "pch.hpp"
#include "event.hpp"

namespace detail {

Pool<RawChunk<4>> gPool4{128u};
Pool<RawChunk<16>> gPool16{128u};

}

NullEventBus gNullEventBus{};