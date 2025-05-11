#ifndef __stdafx_HPP
#define __stdafx_HPP

#include <iostream>
#include <system_error>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <vector>
#include <list>
#include <ranges>
#include <algorithm>
#include <memory>
#include <cstdint>
#include <numeric>
#include <limits>
#include <array>
#include <deque>
#include <optional>
#include <forward_list>
#include <array>
#include <thread>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <latch>
#include <type_traits>
#include <memory_resource>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <concepts>
#include <iterator>
#include <random>
#include <coroutine>
#include <functional>
#include <exception>
#include <string>
#include <sstream>
#include <string_view>

using Seconds = std::chrono::duration<float>;
using SecondsI = std::chrono::seconds;
using MilliSeconds = std::chrono::duration<float, std::milli>;
using MilliSecondsI = std::chrono::milliseconds;
using Clock = std::chrono::system_clock;

inline constexpr MilliSeconds operator"" _ms(unsigned long long int ms) {
    return MilliSeconds(static_cast<float>(ms));
}
inline constexpr MilliSeconds operator"" _ms(long double ms) {
    return MilliSeconds(static_cast<float>(ms));
}

using i8t = std::int8_t;
using i16t = std::int16_t;
using i32t = std::int32_t;
using i64t = std::int64_t;
using u8t = std::uint8_t;
using u16t = std::uint16_t;
using u32t = std::uint32_t;
using u64t = std::uint64_t;

namespace pmr = std::pmr;

#include <concurrent_vector.h>
#include <concurrent_queue.h>
#include <concurrent_unordered_set.h>
#include <concurrent_unordered_map.h>

namespace cc = concurrency;

template <class T>
using ccVector = cc::concurrent_vector<T, pmr::polymorphic_allocator<T>>;
template <class T>
using ccQueue = cc::concurrent_queue<T, pmr::polymorphic_allocator<T>>;
template <class T>
using ccSet = cc::concurrent_unordered_set<
	T,
	std::hash<T>,
	std::equal_to<T>,
	pmr::polymorphic_allocator<T>
>;
template <class K, class V>
using ccMap = cc::concurrent_unordered_map<
	K,
	V,
	std::hash<K>,
	std::equal_to<K>,
	pmr::polymorphic_allocator<std::pair<const K, V>>
>;

using abool = std::atomic_bool;
using ai8t = std::atomic_int8_t;
using ai16t = std::atomic_int16_t;
using ai32t = std::atomic_int32_t;
using ai64t = std::atomic_int64_t;
using au8t = std::atomic_uint8_t;
using au16t = std::atomic_uint16_t;
using au32t = std::atomic_uint32_t;
using au64t = std::atomic_uint64_t;
using afloat = std::atomic<float>;

#include "enumUtil.hpp"

#endif // __stdafx_HPP
