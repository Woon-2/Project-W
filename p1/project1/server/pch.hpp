#ifndef SERVER_PCH_HPP
#define SERVER_PCH_HPP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif // NOMINMAX

#include "windows.hpp"

#include "macro.hpp"
#include "NetAddress.hpp"
#include "SocketUtils.hpp"

#include "protocol.hpp"

#include <iostream>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <thread>
#include <mutex>
#include <atomic>
#include <string>
#include <vector>
#include <array>
#include <set>
#include <queue>
#include <unordered_map>
#include <functional>
#include <cmath>
#include <algorithm>
#include <ranges>
#include <chrono>

#include "../common/mathUtil.hpp"
#include "../common/log.hpp"

using Nanoseconds = std::chrono::duration<float, std::nano>;
using Microseconds = std::chrono::duration<float, std::micro>;
using Milliseconds = std::chrono::duration<float, std::milli>;
using Seconds = std::chrono::duration<float>;

using SystemClock = std::chrono::system_clock;
using HighResolutionClock = std::chrono::high_resolution_clock;

using i8t = std::int8_t;
using i16t = std::int16_t;
using i32t = std::int32_t;
using i64t = std::int64_t;
using u8t = std::uint8_t;
using u16t = std::uint16_t;
using u32t = std::uint32_t;
using u64t = std::uint64_t;

using XMFLOAT2 = DirectX::XMFLOAT2;
using XMFLOAT3 = DirectX::XMFLOAT3;
using XMFLOAT4 = DirectX::XMFLOAT4;
using XMUINT2 = DirectX::XMUINT2;
using XMUINT3 = DirectX::XMUINT3;
using XMUINT4 = DirectX::XMUINT4;
using XMINT2 = DirectX::XMINT2;
using XMINT3 = DirectX::XMINT3;
using XMINT4 = DirectX::XMINT4;
using XMFLOAT3X3 = DirectX::XMFLOAT3X3;
using XMFLOAT4X4 = DirectX::XMFLOAT4X4;

using namespace std::literals;

#endif // SERVER_PCH_HPP