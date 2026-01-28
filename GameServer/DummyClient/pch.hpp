#ifndef client_pch_hpp
#define client_pch_hpp

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
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <array>
#include <vector>

#include "../LobbyServer/protocol.hpp"
#include "NetAddress.hpp"
#include "SocketUtils.hpp"

#include "../common/concurrentqueue.h"
#include "../common/macro.hpp"
#include "../common/types.hpp"

using namespace std::literals;

#endif	// client_pch_hpp