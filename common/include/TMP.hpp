#ifndef __TMP_HPP
#define __TMP_HPP

#include <type_traits>

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

#endif  // __TMP_HPP