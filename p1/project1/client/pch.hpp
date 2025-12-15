#ifndef __PCF_HPP
#define __PCF_HPP

#define _CRT_SECURE_NO_WARNINGS

// #define UNICODE
// #define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOGDICAPMASKS
#define NOSYSMETRICS
#define NOMENUS
#define NOICONS
#define NOSYSCOMMANDS
#define NORASTEROPS
#define OEMRESOURCE
#define NOATOM
#define NOCLIPBOARD
#define NOCOLOR
#define NOCTLMGR
#define NODRAWTEXT
#define NOKERNEL
#define NONLS
#define NOMEMMGR
#define NOMETAFILE
#define NOOPENFILE
#define NOSCROLL
#define NOSERVICE
#define NOSOUND
#define NOTEXTMETRIC
#define NOWH
#define NOCOMM
#define NOKANJI
#define NOHELP
#define NOPROFILER
#define NODEFERWINDOWPOS
#define NOMCX
#define NORPC
#define NOPROXYSTUB
#define NOIMAGE
#define NOTAPE
#define NOMINMAX
#define STRICT

#define DXGI_DEBUG_INFO		// DXGI에서 발생한 예외 정보들을 출력할 경우 활성화

#include <Windows.h>
#include <dxgi1_6.h>
#include "d3dx12/include/directx/d3dx12.h"
#include "d3dx12/include/directx/d3d12.h"
#include "texloader/DDSTextureLoader12.h"
#include <d3dcompiler.h>

#include <DirectXMath.h>

#ifdef DXGI_DEBUG_INFO
#include <dxgidebug.h>
#endif

#include <wrl.h>

#include <iostream>
#include <locale>
#include <string>
#include <string_view>
#include <sstream>
#include <vector>
#include <queue>
#include <list>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <numeric>
#include <ranges>
#include <concepts>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <utility>
#include <cstdint>
#include <type_traits>
#include <chrono>
#include <random>
#include <thread>
#include <latch>
#include <array>
#include <mutex>
#include <span>

#include "mathUtil.hpp"
#include "function.hpp"
#include "threadPool.hpp"
#include "log.hpp"
#include "pool.hpp"

#include "concurrentqueue.h"

#include "windows.hpp"
#include "macro.hpp"
#include "NetAddress.hpp"
#include "SocketUtils.hpp"
#include "protocol.hpp"

#undef min
#undef max
#undef near
#undef far

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "D3DCompiler.lib")

using namespace std::literals;

template <class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

using i8t = std::int8_t;
using i16t = std::int16_t;
using i32t = std::int32_t;
using i64t = std::int64_t;
using u8t = std::uint8_t;
using u16t = std::uint16_t;
using u32t = std::uint32_t;
using u64t = std::uint64_t;

using Nanoseconds = std::chrono::duration<float, std::nano>;
using Microseconds = std::chrono::duration<float, std::micro>;
using Milliseconds = std::chrono::duration<float, std::milli>;
using Seconds = std::chrono::duration<float>;

using SystemClock = std::chrono::system_clock;
using HighResolutionClock = std::chrono::high_resolution_clock;

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

extern std::mt19937 gRandomEngine;

template <class TEnum>
constexpr std::underlying_type_t<TEnum> etoi(TEnum e) {
	return static_cast<std::underlying_type_t<TEnum>>(e);
}

template <std::floating_point T, class TDist = std::uniform_real_distribution<T>>
inline T rand(T closedBegin, T closedEnd) {
	auto rng = TDist(closedBegin, closedEnd);
	return rng(gRandomEngine);
}

template <std::integral T, class TDist = std::uniform_int_distribution<T>>
inline T rand(T closedBegin, T closedEnd) {
	auto rng = TDist(closedBegin, closedEnd);
	return rng(gRandomEngine);
}

template <typename T>
class PassKey { friend T;    PassKey() = default; };

size_t numberOfPhysicalCores() noexcept;

#endif	// __PCF_HPP