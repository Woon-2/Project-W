#ifndef __TMP_HPP
#define __TMP_HPP

#include <type_traits>
#include <concepts>
#include <ranges>
#include <utility>
#include <iostream>

template <class T>
consteval int indexOf() {
    return -1;
}

template <class T, class First, class ... Rest>
consteval int indexOf() {
    if constexpr (std::is_same_v<T, First>) {
        return 0; // 첫 번째 타입이 T와 같으면 0 반환
    } else {
        constexpr int next = indexOf<T, Rest...>();
        return next == -1 ? -1 : 1 + next; // 재귀적으로 인덱스를 계산
    }
}

template <class R>
concept reservable_range = std::ranges::range<R>
    && requires (R& _r, std::size_t _n) {
        _r.reserve(_n);
    };

template <std::ranges::range R>
[[maybe_unused]] R&& reserve_if_possible(R&& range, std::size_t newCapicity) {
    if constexpr ( reservable_range<R> ) {
        range.reserve(newCapicity);
    }
    return std::forward<R>(range);
}

template <class T, class CharT, class Traits>
std::basic_istream<CharT, Traits>& readStream(std::basic_istream<CharT, Traits>& is, T& t) {
    return is.read(reinterpret_cast<char*>(&t), sizeof(T));
}

template <class CharT, class Traits, std::integral IntT>
std::basic_istream<CharT, Traits>& readStream(
    std::basic_istream<CharT, Traits>& is, char* pStr, IntT nStrLength
) {
    is.read(pStr, static_cast<std::streamsize>(nStrLength));
    pStr[nStrLength] = '\0';
    return is;
}

#endif  // __TMP_HPP