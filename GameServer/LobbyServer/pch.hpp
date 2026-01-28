#ifndef lobby_server_pch_hpp
#define lobby_server_pch_hpp

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "simpleWindows.hpp"

#include <iostream>
#include <cstdint>
#include <cassert>
#include <system_error>
#include <string>
#include <string_view>
#include <numeric>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <array>
#include <vector>
#include <map>
#include <unordered_map>

#include "protocol.hpp"
#include "NetAddress.hpp"
#include "SocketUtils.hpp"
#include "RecvBuffer.hpp"
#include "globalTLS.hpp"

#include "../common/concurrentqueue.h"
#include "../common/macro.hpp"
#include "../common/types.hpp"

using namespace std::literals;

size_t numberOfPhysicalCores() noexcept;

#endif	// lobby_server_pch_hpp