#ifndef __TMP_HPP
#define __TMP_HPP

#include <type_traits>
#include <concepts>
#include <ranges>
#include <utility>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <typeinfo>
#include <chrono>

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
    is.read(reinterpret_cast<char*>(&t), sizeof(T));
    if (is.fail() || is.bad()) {
        throw std::runtime_error("[Description] readStream: I/O error occured.");
    }
    return is;
}

template <class CharT, class Traits, std::integral IntT>
std::basic_istream<CharT, Traits>& readStream(
    std::basic_istream<CharT, Traits>& is, char* pStr, IntT nStrLength
) {
    is.read(pStr, static_cast<std::streamsize>(nStrLength));
    pStr[nStrLength] = '\0';
    if (is.fail() || is.bad()) {
        throw std::runtime_error("[Description] readStream: I/O error occured.");
    }
    return is;
}

class AnyMoveOnly {
public:
    AnyMoveOnly() = default;

    // 소유권 이동 가능
    AnyMoveOnly(AnyMoveOnly&& other) noexcept = default;
    AnyMoveOnly& operator=(AnyMoveOnly&& other) noexcept = default;

    // 복사 금지
    AnyMoveOnly(const AnyMoveOnly&) = delete;
    AnyMoveOnly& operator=(const AnyMoveOnly&) = delete;

    template <class T>
    AnyMoveOnly(T&& value) {
        using U = std::decay_t<T>;
        static_assert(!std::is_reference<U>::value, "Can't store reference");
        holder = std::make_unique<Holder<U>>(std::forward<T>(value));
    }

    bool has_value() const {
        return holder != nullptr;
    }

    const std::type_info& type() const {
        return holder ? holder->type() : typeid(void);
    }

    void reset() {
        holder.reset();
    }

    template <class T>
    friend T* any_cast(AnyMoveOnly*) noexcept;
    template <class T>
    friend const T* any_cast(const AnyMoveOnly*) noexcept;
    template <class T>
    friend T&& any_cast_move(AnyMoveOnly&& a);
    

private:
    struct Base {
        virtual ~Base() = default;
        virtual const std::type_info& type() const = 0;
    };

    template <class T>
    struct Holder : Base {
        T value;

        template <class U>
        Holder(U&& v) : value(std::forward<U>(v)) {}

        const std::type_info& type() const override {
            return typeid(T);
        }
    };

    std::unique_ptr<Base> holder;
};

// any_cast: pointer version
template <class T>
T* any_cast(AnyMoveOnly* a) noexcept {
    if (a && a->holder && a->holder->type() == typeid(T)) {
        return &static_cast<AnyMoveOnly::Holder<T>*>(a->holder.get())->value;
    }
    return nullptr;
}

// any_cast: pointer version
template <class T>
const T* any_cast(const AnyMoveOnly* a) noexcept {
    if (a && a->holder && a->holder->type() == typeid(T)) {
        return &static_cast<AnyMoveOnly::Holder<T>*>(a->holder.get())->value;
    }
    return nullptr;
}

template <typename T>
T&& any_cast_move(AnyMoveOnly&& a) {
    if (a.type() != typeid(T)) {
        throw std::bad_cast();
    }
    return std::move(static_cast<AnyMoveOnly::Holder<T>*>(a.holder.get())->value);
}


using Clock = std::chrono::high_resolution_clock;
using Milliseconds = std::chrono::duration<float, std::milli>;

inline Milliseconds operator"" _ms(unsigned long long int ms) {
    return Milliseconds(static_cast<float>(ms));
}
inline Milliseconds operator"" _ms(long double ms) {
    return Milliseconds(static_cast<float>(ms));
}

#endif  // __TMP_HPP