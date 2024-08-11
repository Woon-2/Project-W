#ifndef __MathUtil_HPP
#define __MathUtil_HPP

#define __MathUtil_NOEXCEPT noexcept

#include <cmath>
#include <algorithm>
#include <concepts>

#ifdef ASSIMP_MATH_UTIL
#include "assimp/vector2.h"
#include "assimp/vector3.h"
#include "assimp/matrix3x3.h"
#include "assimp/matrix4x4.h"
#endif

namespace mu {

inline constexpr auto pi = 3.1415926f;

class Degree;

class Radian {
public:
    Radian() __MathUtil_NOEXCEPT
        : Radian(0.f) {}

    Radian(float val) __MathUtil_NOEXCEPT
        : val_(val) {}

    Radian(Degree deg) __MathUtil_NOEXCEPT;

    Radian& operator+=(Radian rhs) __MathUtil_NOEXCEPT {
        val_ += rhs.val_;
        val_ = fmod(val_, 2 * pi);
        return *this;
    }

    Radian& operator-=(Radian rhs) __MathUtil_NOEXCEPT {
        val_ -= rhs.val_;
        val_ = fmod(val_, 2 * pi);
        return *this;
    }

    Radian& operator*=(Radian rhs) __MathUtil_NOEXCEPT {
        val_ *= rhs.val_;
        val_ = fmod(val_, 2 * pi);
        return *this;
    }

    Radian& operator/=(Radian rhs) __MathUtil_NOEXCEPT {
        val_ /= rhs.val_;
        val_ = fmod(val_, 2 * pi);
        return *this;
    }

    friend auto operator<=>(Radian lhs, Radian rhs) __MathUtil_NOEXCEPT = default;

    operator float() const __MathUtil_NOEXCEPT {
        return val_;
    }

    operator Degree() const __MathUtil_NOEXCEPT;

private:
    float val_;
};

class Degree {
public:
    Degree() __MathUtil_NOEXCEPT
        : Degree(0.f) {}

    Degree(float val) __MathUtil_NOEXCEPT
        : val_(val) {}

    Degree(Radian rad) __MathUtil_NOEXCEPT
        : val_(static_cast<float>(rad) * 180.f / pi) {}

    Degree& operator+=(Degree rhs) __MathUtil_NOEXCEPT {
        val_ += rhs.val_;
        val_ = fmod(val_, 360.f);
        return *this;
    }

    Degree& operator-=(Degree rhs) __MathUtil_NOEXCEPT {
        val_ -= rhs.val_;
        val_ = fmod(val_, 360.f);
        return *this;
    }

    Degree& operator*=(Degree rhs) __MathUtil_NOEXCEPT {
        val_ *= rhs.val_;
        val_ = fmod(val_, 360.f);
        return *this;
    }

    Degree& operator/=(Degree rhs) __MathUtil_NOEXCEPT {
        val_ /= rhs.val_;
        val_ = fmod(val_, 360.f);
        return *this;
    }

    friend auto operator<=>(Degree lhs, Degree rhs) __MathUtil_NOEXCEPT = default;

    operator float() const __MathUtil_NOEXCEPT {
        return val_;
    }

    operator Radian() const __MathUtil_NOEXCEPT {
        return val_ * pi / 180.f;
    }

private:
    float val_;
};

inline Radian::Radian(Degree deg) __MathUtil_NOEXCEPT
    : val_(static_cast<float>(deg) * pi / 180.f) {}

inline Radian::operator Degree() const __MathUtil_NOEXCEPT {
    return val_ * 180.f / pi;
}

inline Radian operator+(Radian lhs, Radian rhs) __MathUtil_NOEXCEPT {
    return lhs += rhs;
}

inline Radian operator-(Radian lhs, Radian rhs) __MathUtil_NOEXCEPT {
    return lhs -= rhs;
}

inline Radian operator*(Radian lhs, Radian rhs) __MathUtil_NOEXCEPT {
    return lhs *= rhs;
}

inline Radian operator/(Radian lhs, Radian rhs) __MathUtil_NOEXCEPT {
    return lhs /= rhs;
}

inline Degree operator+(Degree lhs, Degree rhs) __MathUtil_NOEXCEPT {
    return lhs += rhs;
}

inline Degree operator-(Degree lhs, Degree rhs) __MathUtil_NOEXCEPT {
    return lhs -= rhs;
}

inline Degree operator*(Degree lhs, Degree rhs) __MathUtil_NOEXCEPT {
    return lhs *= rhs;
}

inline Degree operator/(Degree lhs, Degree rhs) __MathUtil_NOEXCEPT {
    return lhs /= rhs;
}

#if defined(DXMATH_VEC_UTIL) || defined(DXMATH_MAT_UTIL) || defined(DXMATH_QUAT_UTIL)
#define MU_CALLCONV XM_CALLCONV
#else
#define MU_CALLCONV
#endif

#ifdef DXMATH_VEC_UTIL
#include <DirectXMath.h>
namespace dx = DirectX;

template <std::size_t D>
    requires (D >= 1 && D <= 4)
class NVec;

#ifdef DXMATH_MAT_UTIL
template <std::size_t R, std::size_t C>
    requires (R >= 1 && R <= 4) && (C >= 1 && C <= 4)
class Mat;
#endif  // DXMATH_MAT_UTIL

template <std::size_t D /* Dimension */>
    requires (D >= 1 && D <= 4)
class alignas(16) Vec {
public:
    template <std::size_t D2>
        requires (D2 >= 1 && D2 <= 4)
    friend class Vec;

    template <std::size_t D2>
        requires (D2 >= 1 && D2 <= 4)
    friend class NVec;

    Vec() __MathUtil_NOEXCEPT
        : Vec(dx::XMVectorZero()) {}

    Vec(float val) __MathUtil_NOEXCEPT
        : Vec(dx::XMVectorReplicate(val)) {}

    Vec(float x, float y) __MathUtil_NOEXCEPT requires (D == 2)
        : Vec(dx::XMVectorSet(x, y, 0.f, 1.f)) {}

    Vec(float x, float y, float z) __MathUtil_NOEXCEPT requires (D == 3)
        : Vec(dx::XMVectorSet(x, y, z, 1.f)) {}

    Vec(float x, float y, float z, float w) __MathUtil_NOEXCEPT requires (D == 4)
        : Vec(dx::XMVectorSet(x, y, z, w)) {}

    Vec(dx::FXMVECTOR vec) __MathUtil_NOEXCEPT
        : vec_(vec) {}

    template <std::size_t D2>
    Vec(Vec<D2> vec) __MathUtil_NOEXCEPT
        : vec_(vec.vec_) {}

    template <std::size_t D2>
    Vec(NVec<D2> vec) __MathUtil_NOEXCEPT;

#ifdef ASSIMP_MATH_UTIL
    template <std::floating_point Fl>
    Vec(aiVector2t<Fl> vec) __MathUtil_NOEXCEPT
        : Vec(vec.x, vec.y) {}

    template <std::floating_point Fl>
    Vec(aiVector3t<Fl> vec) __MathUtil_NOEXCEPT
        : Vec(vec.x, vec.y, vec.z) {}
#endif  // ASSIMP_MATH_UTIL

    template <std::size_t D2, std::floating_point ... Fs>
        requires (D2 >= 1 && D2 < 4 && D > D2)
    Vec(Vec<D2> vec, Fs... floats) __MathUtil_NOEXCEPT
        : vec_(vec.vec_) {
        if constexpr (D2 == 1) {
            const auto ctrl = dx::XMVectorSet(0.f, 1.f, 1.f, 1.f);
            vec_ = dx::XMVectorSelect(vec_, dx::XMVectorSet(0.f, floats...), ctrl);
        }
        else if constexpr (D2 == 2) {
            const auto ctrl = dx::XMVectorSet(0.f, 0.f, 1.f, 1.f);
            vec_ = dx::XMVectorSelect(vec_, dx::XMVectorSet(0.f, 0.f, floats...), ctrl);
        }
        else {
            const auto ctrl = dx::XMVectorSet(0.f, 0.f, 0.f, 1.f);
            vec_ = dx::XMVectorSelect(vec_, dx::XMVectorSet(0.f, 0.f, 0.f, floats...), ctrl);
        }
    }

    template <std::size_t D2, std::floating_point ... Fs>
        requires (D2 >= 1 && D2 < 4 && D > D2)
    Vec(NVec<D2> vec, Fs... floats) __MathUtil_NOEXCEPT;

    Vec& XM_CALLCONV operator+=(Vec rhs) __MathUtil_NOEXCEPT {
        using dx::operator+=;
        vec_ += rhs.vec_;
        return *this;
    }

    Vec& XM_CALLCONV operator-=(Vec rhs) __MathUtil_NOEXCEPT {
        using dx::operator-=;
        vec_ -= rhs.vec_;
        return *this;
    }

    Vec& XM_CALLCONV operator*=(Vec rhs) __MathUtil_NOEXCEPT {
        using dx::operator*=;
        vec_ *= rhs.vec_;
        return *this;
    }

    Vec& XM_CALLCONV operator/=(Vec rhs) __MathUtil_NOEXCEPT {
        using dx::operator/=;
        vec_ /= rhs.vec_;
        return *this;
    }

    Vec& XM_CALLCONV operator+=(float rhs) __MathUtil_NOEXCEPT {
        return *this += Vec(rhs);
    }

    Vec& XM_CALLCONV operator-=(float rhs) __MathUtil_NOEXCEPT {
        return *this -= Vec(rhs);
    }

    Vec& XM_CALLCONV operator*=(float rhs) __MathUtil_NOEXCEPT {
        return *this *= Vec(rhs);
    }

    Vec& XM_CALLCONV operator/=(float rhs) __MathUtil_NOEXCEPT {
        return *this /= Vec(rhs);
    }

    Vec& XM_CALLCONV operator+=(NVec<D> rhs) __MathUtil_NOEXCEPT;

    Vec& XM_CALLCONV operator-=(NVec<D> rhs) __MathUtil_NOEXCEPT;

    Vec& XM_CALLCONV operator*=(NVec<D> rhs) __MathUtil_NOEXCEPT;

    Vec& XM_CALLCONV operator/=(NVec<D> rhs) __MathUtil_NOEXCEPT;

#ifdef DXMATH_MAT_UTIL
    Vec& XM_CALLCONV operator*=(Mat<D, D> rhs) __MathUtil_NOEXCEPT;
#endif  // DXMATH_MAT_UTIL

    dx::XMVECTOR& get() __MathUtil_NOEXCEPT {
        return vec_;
    }

    const dx::XMVECTOR& get() const __MathUtil_NOEXCEPT {
        return vec_;
    }

    float XM_CALLCONV x() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetX(vec_);
    }

    float XM_CALLCONV y() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetY(vec_);
    }

    float XM_CALLCONV z() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetZ(vec_);
    }

    float XM_CALLCONV r() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetX(vec_);
    }

    float XM_CALLCONV g() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetY(vec_);
    }

    float XM_CALLCONV b() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetZ(vec_);
    }

    float XM_CALLCONV w() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetW(vec_);
    }

    float XM_CALLCONV operator[](std::size_t idx) const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetByIndex(vec_, idx);
    }

    float XM_CALLCONV len() const __MathUtil_NOEXCEPT {
        if constexpr (D == 1) {
            return dx::XMVectorGetX(vec_);
        } else if constexpr (D == 2) {
            return dx::XMVectorGetX(dx::XMVector2Length(vec_));
        } else if constexpr (D == 3) {
            return dx::XMVectorGetX(dx::XMVector3Length(vec_));
        } else if constexpr (D == 4) {
            return dx::XMVectorGetX(dx::XMVector4Length(vec_));
        }
    }

    float XM_CALLCONV len2() const __MathUtil_NOEXCEPT {
        if constexpr (D == 1) {
            return dx::XMVectorGetX(vec_) * dx::XMVectorGetX(vec_);
        } else if constexpr (D == 2) {
            return dx::XMVectorGetX(dx::XMVector2LengthSq(vec_));
        } else if constexpr (D == 3) {
            return dx::XMVectorGetX(dx::XMVector3LengthSq(vec_));
        } else if constexpr (D == 4) {
            return dx::XMVectorGetX(dx::XMVector4LengthSq(vec_));
        }
    }

    float XM_CALLCONV norm() const __MathUtil_NOEXCEPT {
        return len2();
    }

    template <std::size_t D2>
        requires (D2 >= 1 && D2 <= 4)
    operator NVec<D2>() const __MathUtil_NOEXCEPT;

    const Vec XM_CALLCONV operator-() const __MathUtil_NOEXCEPT {
        return Vec(dx::XMVectorNegate(vec_));
    }

private:
    dx::XMVECTOR vec_;
};

template <std::size_t D /* Dimension */>
    requires (D >= 1 && D <= 4)
class alignas(16) NVec {
public:
    template <std::size_t D2>
        requires (D2 >= 1 && D2 <= 4)
    friend class Vec;

    template <std::size_t D2>
        requires (D2 >= 1 && D2 <= 4)
    friend class NVec;

    NVec() __MathUtil_NOEXCEPT
        : NVec(1.f) {
        normalize();
    }

    NVec(float val) __MathUtil_NOEXCEPT
        : vec_(dx::XMVectorReplicate(val)) {
        normalize();
    }

    NVec(float x, float y) __MathUtil_NOEXCEPT requires (D == 2)
        : vec_(dx::XMVectorSet(x, y, 0.f, 1.f)) {
        normalize();
    }

    NVec(float x, float y, float z) __MathUtil_NOEXCEPT requires (D == 3)
        : vec_(dx::XMVectorSet(x, y, z, 1.f)) {
        normalize();
    }

    NVec(float x, float y, float z, float w) __MathUtil_NOEXCEPT requires (D == 4)
        : vec_(dx::XMVectorSet(x, y, z, w)) {
        normalize();
    }

    NVec(dx::FXMVECTOR vec) __MathUtil_NOEXCEPT
        : vec_(vec) {
        normalize();
    }

    template <std::size_t D2>
        requires (D2 >= 1 && D2 <= 4)
    NVec(Vec<D2> vec) __MathUtil_NOEXCEPT
        : vec_(vec.vec_) {
        normalize();
    }

    template <std::size_t D2, std::floating_point ... Fs>
        requires (D2 >= 1 && D2 < 4 && D > D2)
    NVec(Vec<D2> vec, Fs... floats) __MathUtil_NOEXCEPT
        : vec_(vec.vec_) {
        if constexpr (D2 == 1) {
            const auto ctrl = dx::XMVectorSet(0.f, 1.f, 1.f, 1.f);
            vec_ = dx::XMVectorSelect(vec_, dx::XMVectorSet(0.f, floats...), ctrl);
        }
        else if constexpr (D2 == 2) {
            const auto ctrl = dx::XMVectorSet(0.f, 0.f, 1.f, 1.f);
            vec_ = dx::XMVectorSelect(vec_, dx::XMVectorSet(0.f, 0.f, floats...), ctrl);
        }
        else {
            const auto ctrl = dx::XMVectorSet(0.f, 0.f, 0.f, 1.f);
            vec_ = dx::XMVectorSelect(vec_, dx::XMVectorSet(0.f, 0.f, 0.f, floats...), ctrl);
        }

        normalize();
    }

    template <std::size_t D2, std::floating_point ... Fs>
        requires (D2 >= 1 && D2 < 4 && D > D2)
    NVec(NVec<D2> vec, Fs... floats) __MathUtil_NOEXCEPT
        : vec_(vec.vec_) {
        if constexpr (D2 == 1) {
            const auto ctrl = dx::XMVectorSet(0.f, 1.f, 1.f, 1.f);
            vec_ = dx::XMVectorSelect(vec_, dx::XMVectorSet(0.f, floats...), ctrl);
        }
        else if constexpr (D2 == 2) {
            const auto ctrl = dx::XMVectorSet(0.f, 0.f, 1.f, 1.f);
            vec_ = dx::XMVectorSelect(vec_, dx::XMVectorSet(0.f, 0.f, floats...), ctrl);
        }
        else {
            const auto ctrl = dx::XMVectorSet(0.f, 0.f, 0.f, 1.f);
            vec_ = dx::XMVectorSelect(vec_, dx::XMVectorSet(0.f, 0.f, 0.f, floats...), ctrl);
        }
    }

#ifdef ASSIMP_MATH_UTIL
    template <std::floating_point Fl>
    NVec(aiVector2t<Fl> vec) __MathUtil_NOEXCEPT
        : NVec(vec.x, vec.y) {}

    template <std::floating_point Fl>
    NVec(aiVector3t<Fl> vec) __MathUtil_NOEXCEPT
        : NVec(vec.x, vec.y, vec.z) {}
#endif // ASSIMP_MATH_UTIL

    dx::XMVECTOR& get() __MathUtil_NOEXCEPT {
        return vec_;
    }

    const dx::XMVECTOR& get() const __MathUtil_NOEXCEPT {
        return vec_;
    }

    float XM_CALLCONV x() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetX(vec_);
    }

    float XM_CALLCONV y() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetY(vec_);
    }

    float XM_CALLCONV z() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetZ(vec_);
    }

    float XM_CALLCONV r() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetX(vec_);
    }

    float XM_CALLCONV g() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetY(vec_);
    }

    float XM_CALLCONV b() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetZ(vec_);
    }

    float XM_CALLCONV w() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetW(vec_);
    }

    float XM_CALLCONV operator[](std::size_t idx) const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetByIndex(vec_, idx);
    }

    constexpr float XM_CALLCONV len() const __MathUtil_NOEXCEPT {
        return 1.f;
    }

    constexpr float XM_CALLCONV len2() const __MathUtil_NOEXCEPT {
        return 1.f;
    }

    constexpr float XM_CALLCONV norm() const __MathUtil_NOEXCEPT {
        return 1.f;
    }

    template <std::size_t D2>
        requires (D2 >= 1 && D2 <= 4)
    operator Vec<D2>() const __MathUtil_NOEXCEPT {
        return Vec<D2>(vec_);
    }

    operator dx::XMVECTOR() const __MathUtil_NOEXCEPT {
        return vec_;
    }

    const Vec<D> XM_CALLCONV operator-() const __MathUtil_NOEXCEPT {
        return Vec<D>(dx::XMVectorNegate(vec_));
    }

private:
    void normalize() __MathUtil_NOEXCEPT {
        if constexpr (D == 1) {
            vec_ = dx::XMVectorReplicate(1.f);
        } else if constexpr (D == 2) {
            vec_ = dx::XMVector2Normalize(vec_);
        } else if constexpr (D == 3) {
            vec_ = dx::XMVector3Normalize(vec_);
        } else if constexpr (D == 4) {
            vec_ = dx::XMVector4Normalize(vec_);
        }
    }

    dx::XMVECTOR vec_;
};

template <std::size_t D>
    requires (D >= 1 && D <= 4)
template <std::size_t D2, std::floating_point ... Fs>
    requires (D2 >= 1 && D2 < 4 && D > D2)
Vec<D>::Vec(NVec<D2> vec, Fs... floats) __MathUtil_NOEXCEPT
    : vec_(vec) {
    if constexpr (D2 == 1) {
        const auto ctrl = dx::XMVectorSet(0.f, 1.f, 1.f, 1.f);
        vec_ = dx::XMVectorSelect(vec_, dx::XMVectorSet(0.f, floats...), ctrl);
    }
    else if constexpr (D2 == 2) {
        const auto ctrl = dx::XMVectorSet(0.f, 0.f, 1.f, 1.f);
        vec_ = dx::XMVectorSelect(vec_, dx::XMVectorSet(0.f, 0.f, floats...), ctrl);
    }
    else {
        const auto ctrl = dx::XMVectorSet(0.f, 0.f, 0.f, 1.f);
        vec_ = dx::XMVectorSelect(vec_, dx::XMVectorSet(0.f, 0.f, 0.f, floats...), ctrl);
    }
}

template <std::size_t D> requires (D >= 1 && D <= 4)
template <std::size_t D2>
Vec<D>::Vec(NVec<D2> vec) __MathUtil_NOEXCEPT
    : vec_(vec.vec_) {}

template <std::size_t D> requires (D >= 1 && D <= 4)
template <std::size_t D2> requires (D2 >= 1 && D2 <= 4)
Vec<D>::operator NVec<D2>() const __MathUtil_NOEXCEPT {
    return NVec<D2>(vec_);
}

template <std::size_t D> requires (D >= 1 && D <= 4)
Vec<D>& XM_CALLCONV Vec<D>::operator+=(NVec<D> rhs) __MathUtil_NOEXCEPT {
    using dx::operator+=;
    vec_ += rhs.vec_;
    return *this;
}

template <std::size_t D> requires (D >= 1 && D <= 4)
Vec<D>& XM_CALLCONV Vec<D>::operator-=(NVec<D> rhs) __MathUtil_NOEXCEPT {
    using dx::operator-=;
    vec_ -= rhs.vec_;
    return *this;
}

template <std::size_t D> requires (D >= 1 && D <= 4)
Vec<D>& XM_CALLCONV Vec<D>::operator*=(NVec<D> rhs) __MathUtil_NOEXCEPT {
    using dx::operator*=;
    vec_ *= rhs.vec_;
    return *this;
}

template <std::size_t D> requires (D >= 1 && D <= 4)
Vec<D>& XM_CALLCONV Vec<D>::operator/=(NVec<D> rhs) __MathUtil_NOEXCEPT {
    using dx::operator/=;
    vec_ /= rhs.vec_;
    return *this;
}

template <std::size_t D>
const Vec<D> XM_CALLCONV operator+(Vec<D> lhs, Vec<D> rhs) __MathUtil_NOEXCEPT {
    return dx::XMVectorAdd(lhs.get(), rhs.get());
}

template <std::size_t D>
const Vec<D> XM_CALLCONV operator-(Vec<D> lhs, Vec<D> rhs) __MathUtil_NOEXCEPT {
    return dx::XMVectorSubtract(lhs.get(), rhs.get());
}

template <std::size_t D>
const Vec<D> XM_CALLCONV operator*(Vec<D> lhs, Vec<D> rhs) __MathUtil_NOEXCEPT {
    return dx::XMVectorMultiply(lhs.get(), rhs.get());
}

template <std::size_t D>
const Vec<D> XM_CALLCONV operator/(Vec<D> lhs, Vec<D> rhs) __MathUtil_NOEXCEPT {
    return dx::XMVectorDivide(lhs.get(), rhs.get());
}

template <std::size_t D>
const Vec<D> XM_CALLCONV operator+(Vec<D> lhs, float rhs) __MathUtil_NOEXCEPT {
    return lhs += Vec<D>(rhs);
}

template <std::size_t D>
const Vec<D> XM_CALLCONV operator-(Vec<D> lhs, float rhs) __MathUtil_NOEXCEPT {
    return lhs -= Vec<D>(rhs);
}

template <std::size_t D>
const Vec<D> XM_CALLCONV operator*(Vec<D> lhs, float rhs) __MathUtil_NOEXCEPT {
    return lhs *= Vec<D>(rhs);
}

template <std::size_t D>
const Vec<D> XM_CALLCONV operator*(float lhs, Vec<D> rhs) __MathUtil_NOEXCEPT {
    return rhs * lhs;
}

template <std::size_t D>
const Vec<D> XM_CALLCONV operator/(Vec<D> lhs, float rhs) __MathUtil_NOEXCEPT {
    return lhs /= Vec<D>(rhs);
}

template <std::size_t D>
const Vec<D> XM_CALLCONV operator+(Vec<D> lhs, NVec<D> rhs) __MathUtil_NOEXCEPT {
    return lhs += rhs;
}

template <std::size_t D>
const Vec<D> XM_CALLCONV operator-(Vec<D> lhs, NVec<D> rhs) __MathUtil_NOEXCEPT {
    return lhs -= rhs;
}

template <std::size_t D>
const Vec<D> XM_CALLCONV operator*(Vec<D> lhs, NVec<D> rhs) __MathUtil_NOEXCEPT {
    return lhs *= rhs;
}

template <std::size_t D>
const Vec<D> XM_CALLCONV operator/(Vec<D> lhs, NVec<D> rhs) __MathUtil_NOEXCEPT {
    return lhs /= rhs;
}

template <std::size_t D>
const Vec<D> XM_CALLCONV operator+(NVec<D> lhs, Vec<D> rhs) __MathUtil_NOEXCEPT {
    return rhs + lhs;
}

template <std::size_t D>
const Vec<D> XM_CALLCONV operator-(NVec<D> lhs, Vec<D> rhs) __MathUtil_NOEXCEPT {
    return rhs - lhs;
}

template <std::size_t D>
const Vec<D> XM_CALLCONV operator*(NVec<D> lhs, Vec<D> rhs) __MathUtil_NOEXCEPT {
    return rhs * lhs;
}

template <std::size_t D>
const Vec<D> XM_CALLCONV operator/(NVec<D> lhs, Vec<D> rhs) __MathUtil_NOEXCEPT {
    return rhs / lhs;
}

template <std::size_t D>
bool XM_CALLCONV operator==(Vec<D> lhs, Vec<D> rhs) __MathUtil_NOEXCEPT {
    if constexpr (D == 1) {
        return dx::XMVectorGetX(lhs.get()) == dx::XMVectorGetX(rhs.get());
    } else if constexpr (D == 2) {
        return dx::XMVector2Equal(lhs.get(), rhs.get());
    } else if constexpr (D == 3) {
        return dx::XMVector3Equal(lhs.get(), rhs.get());
    } else if constexpr (D == 4) {
        return dx::XMVector4Equal(lhs.get(), rhs.get());
    }
}

template <std::size_t D>
bool XM_CALLCONV nearEqual(Vec<D> lhs, Vec<D> rhs, Vec<D> epsilon) __MathUtil_NOEXCEPT {
    if constexpr (D == 1) {
        return dx::XMScalarNearEqual(
            dx::XMVectorGetX(lhs.get()), dx::XMVectorGetX(rhs.get()),
            dx::XMVectorGetX(epsilon.get())
        );
    } else if constexpr (D == 2) {
        return dx::XMVector2NearEqual(lhs.get(), rhs.get(), epsilon.get());
    } else if constexpr (D == 3) {
        return dx::XMVector3NearEqual(lhs.get(), rhs.get(), epsilon.get());
    } else if constexpr (D == 4) {
        return dx::XMVector4NearEqual(lhs.get(), rhs.get(), epsilon.get());
    }
}

template <std::size_t D>
bool XM_CALLCONV nearEqual(Vec<D> lhs, Vec<D> rhs, float epsilon = dx::g_XMEpsilon) __MathUtil_NOEXCEPT {
    return nearEqual(lhs, rhs, Vec<D>(epsilon));
}


template <std::size_t D>
bool XM_CALLCONV operator!=(Vec<D> lhs, Vec<D> rhs) __MathUtil_NOEXCEPT {
    return !(lhs == rhs);
}

template <std::size_t D1, std::size_t D2>
auto XM_CALLCONV operator<=>(Vec<D1> lhs, Vec<D2> rhs) __MathUtil_NOEXCEPT {
    return lhs.norm() <=> rhs.norm();
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
float XM_CALLCONV dot(Vec<D> lhs, Vec<D> rhs) __MathUtil_NOEXCEPT {
    if constexpr (D == 1) {
        return dx::XMVectorGetX(dx::XMVectorMultiply(lhs.get(), rhs.get()));
    } else if constexpr (D == 2) {
        return dx::XMVectorGetX(dx::XMVector2Dot(lhs.get(), rhs.get()));
    } else if constexpr (D == 3) {
        return dx::XMVectorGetX(dx::XMVector3Dot(lhs.get(), rhs.get()));
    } else if constexpr (D == 4) {
        return dx::XMVectorGetX(dx::XMVector4Dot(lhs.get(), rhs.get()));
    }
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV cross(Vec<D> lhs, Vec<D> rhs) __MathUtil_NOEXCEPT {
    if constexpr (D == 2) {
        return dx::XMVector2Cross(lhs.get(), rhs.get());
    } else if constexpr (D == 3) {
        return dx::XMVector3Cross(lhs.get(), rhs.get());
    } else {
        static_assert(D == 2 || D == 3,
            "Cross product is only defined for 2D and 3D vectors."
        );
    }
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const NVec<D> XM_CALLCONV normalize(Vec<D> vec) __MathUtil_NOEXCEPT {
    return NVec<D>(vec);
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const NVec<D> XM_CALLCONV normalizeEst(Vec<D> vec) __MathUtil_NOEXCEPT {
    return NVec<D>(dx::XMVector3NormalizeEst(vec.get()));
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV abs(Vec<D> vec) __MathUtil_NOEXCEPT {
    return dx::XMVectorAbs(vec.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV floor(Vec<D> vec) __MathUtil_NOEXCEPT {
    return dx::XMVectorFloor(vec.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV ceil(Vec<D> vec) __MathUtil_NOEXCEPT {
    return dx::XMVectorCeiling(vec.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV round(Vec<D> vec) __MathUtil_NOEXCEPT {
    return dx::XMVectorRound(vec.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV trunc(Vec<D> vec) __MathUtil_NOEXCEPT {
    return dx::XMVectorTruncate(vec.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV frac(Vec<D> vec) __MathUtil_NOEXCEPT {
    return vec - trunc(vec);
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV select(
    Vec<D> lhs, Vec<D> rhs, Vec<D> control
) __MathUtil_NOEXCEPT {
    return dx::XMVectorSelect(lhs.get(), rhs.get(), control.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV sqrt(Vec<D> vec) __MathUtil_NOEXCEPT {
    return dx::XMVectorSqrt(vec.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV sqrtEst(Vec<D> vec) __MathUtil_NOEXCEPT {
    return dx::XMVectorSqrtEst(vec.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV rsqrt(Vec<D> vec) __MathUtil_NOEXCEPT {
    return dx::XMVectorReciprocalSqrt(vec.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV reflect(
    Vec<D> vec, NVec<D> normal
) __MathUtil_NOEXCEPT {
    return vec - 2.f * dot(vec, normal) * normal;
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV refract(
    Vec<D> vec, NVec<D> normal, float eta
) __MathUtil_NOEXCEPT {
    auto dotVN = dot(vec, normal);
    auto k = 1.f - eta * eta * (1.f - dotVN * dotVN);
    if (k < 0.f) {
        return Vec<D>(0.f);
    } else {
        return eta * vec - (eta * dotVN + sqrt(k)) * normal;
    }
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV lerp(
    Vec<D> lhs, Vec<D> rhs, float t
) __MathUtil_NOEXCEPT {
    return lhs + t * (rhs - lhs);
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV saturate(
    Vec<D> vec
) __MathUtil_NOEXCEPT {
    return dx::XMVectorSaturate(vec.get());
}

// TODO: smoothstep, fade, step, boxstep, smoothmin, smoothmax, smoothminmax, etc.

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV barycentric(
    Vec<D> v1, Vec<D> v2, Vec<D> v3, float f, float g
) __MathUtil_NOEXCEPT {
    return dx::XMVectorBaryCentric(v1.get(), v2.get(), v3.get(), f, g);
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV catmullrom(
    Vec<D> v0, Vec<D> v1, Vec<D> v2, Vec<D> v3, float s
) __MathUtil_NOEXCEPT {
    return dx::XMVectorCatmullRom(v0.get(), v1.get(), v2.get(), v3.get(), s);
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV hermite(
    Vec<D> v1, Vec<D> t1, Vec<D> v2, Vec<D> t2, float s
) __MathUtil_NOEXCEPT {
    return dx::XMVectorHermite(v1.get(), t1.get(), v2.get(), t2.get(), s);
}

template <std::size_t D1, std::size_t D2>
const Vec<std::max(D1, D2)> XM_CALLCONV min(
    Vec<D1> lhs, Vec<D2> rhs
) __MathUtil_NOEXCEPT {
    return dx::XMVectorMin(lhs.get(), rhs.get());
}

template <std::size_t D>
const Vec<D> XM_CALLCONV max(
    Vec<D> lhs, Vec<D> rhs
) __MathUtil_NOEXCEPT {
    return dx::XMVectorMax(lhs.get(), rhs.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV clamp(
    Vec<D> vec, Vec<D> min, Vec<D> max
) __MathUtil_NOEXCEPT {
    return dx::XMVectorClamp(vec.get(), min.get(), max.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV mod(
    Vec<D> vec, Vec<D> divisor
) __MathUtil_NOEXCEPT {
    return dx::XMVectorMod(vec.get(), divisor.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV pow(Vec<D> vec, Vec<D> exp) __MathUtil_NOEXCEPT {
    return dx::XMVectorPow(vec.get(), exp.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV exp(Vec<D> vec) __MathUtil_NOEXCEPT {
    return dx::XMVectorExp(vec.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV log2(Vec<D> vec) __MathUtil_NOEXCEPT {
    return dx::XMVectorLog2(vec.get());
}

#ifdef Win10_20348
template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV log10(Vec<D> vec) __MathUtil_NOEXCEPT {
    return dx::XMVectorLog10(vec.get());
}
#endif

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV logE(Vec<D> vec) __MathUtil_NOEXCEPT {
    return dx::XMVectorLogE(vec.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV less(Vec<D> lhs, Vec<D> rhs) __MathUtil_NOEXCEPT {
    return dx::XMVectorLess(lhs.get(), rhs.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV lessEqual(Vec<D> lhs, Vec<D> rhs) __MathUtil_NOEXCEPT {
    return dx::XMVectorLessOrEqual(lhs.get(), rhs.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV greater(Vec<D> lhs, Vec<D> rhs) __MathUtil_NOEXCEPT {
    return dx::XMVectorGreater(lhs.get(), rhs.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV greaterEqual(Vec<D> lhs, Vec<D> rhs) __MathUtil_NOEXCEPT {
    return dx::XMVectorGreaterOrEqual(lhs.get(), rhs.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV equal(Vec<D> lhs, Vec<D> rhs) __MathUtil_NOEXCEPT {
    return dx::XMVectorEqual(lhs.get(), rhs.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV notEqual(Vec<D> lhs, Vec<D> rhs) __MathUtil_NOEXCEPT {
    return dx::XMVectorNotEqual(lhs.get(), rhs.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV bwAnd(Vec<D> lhs, Vec<D> rhs) __MathUtil_NOEXCEPT {
    return dx::XMVectorAndInt(lhs.get(), rhs.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV bwOr(Vec<D> lhs, Vec<D> rhs) __MathUtil_NOEXCEPT {
    return dx::XMVectorOrInt(lhs.get(), rhs.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV bwXor(Vec<D> lhs, Vec<D> rhs) __MathUtil_NOEXCEPT {
    return dx::XMVectorXorInt(lhs.get(), rhs.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV bwNot(Vec<D> vec) __MathUtil_NOEXCEPT {
    return dx::XMVectorNearEqual(vec.get(), dx::XMVectorZero());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV lshift(Vec<D> lhs, Vec<D> rhs) __MathUtil_NOEXCEPT {
    return dx::XMVectorShiftLeft(lhs.get(), rhs.get());
}

// TODO: rshift, arshift, etc.

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV sin(Vec<D> vec) __MathUtil_NOEXCEPT {
    return dx::XMVectorSin(vec.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV cos(Vec<D> vec) __MathUtil_NOEXCEPT {
    return dx::XMVectorCos(vec.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV tan(Vec<D> vec) __MathUtil_NOEXCEPT {
    return dx::XMVectorTan(vec.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV asin(Vec<D> vec) __MathUtil_NOEXCEPT {
    return dx::XMVectorASin(vec.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV acos(Vec<D> vec) __MathUtil_NOEXCEPT {
    return dx::XMVectorACos(vec.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
const Vec<D> XM_CALLCONV atan(Vec<D> vec) __MathUtil_NOEXCEPT {
    return dx::XMVectorATan(vec.get());
}

using Vec1 = Vec<1>;
using Vec2 = Vec<2>;
using Vec3 = Vec<3>;
using Vec4 = Vec<4>;

using NVec1 = NVec<1>;
using NVec2 = NVec<2>;
using NVec3 = NVec<3>;
using NVec4 = NVec<4>;

#endif  // DXMATH_VEC_UTIL

#ifdef DXMATH_QUAT_UTIL

#ifdef DXMATH_MAT_UTIL
template <std::size_t R, std::size_t C>
    requires (R >= 1 && R <= 4) && (C >= 1 && C <= 4)
class Mat;
#endif  // DXMATH_MAT_UTIL

class Quat {
public:
    friend class NQuat;

    Quat() __MathUtil_NOEXCEPT
        : Quat(dx::XMQuaternionIdentity()) {}

    Quat(float x, float y, float z, float w) __MathUtil_NOEXCEPT
        : Quat(dx::XMVectorSet(x, y, z, w)) {}

    Quat(dx::FXMVECTOR quat) __MathUtil_NOEXCEPT
        : quat_(quat) {}

    Quat(class NQuat nQuat) __MathUtil_NOEXCEPT;

    Quat& XM_CALLCONV operator+=(Quat rhs) __MathUtil_NOEXCEPT {
        using dx::operator+=;
        quat_ += rhs.quat_;
        return *this;
    }

    Quat& XM_CALLCONV operator-=(Quat rhs) __MathUtil_NOEXCEPT {
        using dx::operator-=;
        quat_ -= rhs.quat_;
        return *this;
    }

    Quat& XM_CALLCONV operator*=(Quat rhs) __MathUtil_NOEXCEPT {
        quat_ = dx::XMQuaternionMultiply(quat_, rhs.quat_);
        return *this;
    }

    Quat& XM_CALLCONV operator/=(Quat rhs) __MathUtil_NOEXCEPT {
        quat_ = dx::XMQuaternionMultiply(quat_, dx::XMQuaternionInverse(rhs.quat_));
        return *this;
    }

    dx::XMVECTOR& get() __MathUtil_NOEXCEPT {
        return quat_;
    }

    const dx::XMVECTOR& get() const __MathUtil_NOEXCEPT {
        return quat_;
    }

    const Quat dual() const __MathUtil_NOEXCEPT {
        return Quat(dx::XMQuaternionConjugate(quat_));
    }

    const Quat operator~() const __MathUtil_NOEXCEPT {
        return dual();
    }

    const Quat operator-() const __MathUtil_NOEXCEPT {
        return Quat(dx::XMVectorNegate(quat_));
    }

    float XM_CALLCONV x() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetX(quat_);
    }

    float XM_CALLCONV y() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetY(quat_);
    }

    float XM_CALLCONV z() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetZ(quat_);
    }

    float XM_CALLCONV w() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetW(quat_);
    }

    float XM_CALLCONV operator[](std::size_t idx) const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetByIndex(quat_, idx);
    }

    float XM_CALLCONV len() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetX(dx::XMQuaternionLength(quat_));
    }

    float XM_CALLCONV len2() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetX(dx::XMQuaternionLengthSq(quat_));
    }

    float XM_CALLCONV norm() const __MathUtil_NOEXCEPT {
        return len2();
    }

private:
    dx::XMVECTOR quat_;
};

class NQuat {
private:
    struct NoNormalize_t {};

public:
    friend class Quat;

    NQuat() __MathUtil_NOEXCEPT
        : quat_(dx::XMQuaternionIdentity()) {
        normalize();
    }

    NQuat(float x, float y, float z, float w) __MathUtil_NOEXCEPT
        : NQuat(dx::XMVectorSet(x, y, z, w)) {
        normalize();
    }

    NQuat(Radian angle, float axisX, float axisY, float axisZ) __MathUtil_NOEXCEPT
        : quat_( dx::XMQuaternionRotationAxis(
            dx::XMVectorSet(axisX, axisY, axisZ, 0.f),
        angle) ) {}

    NQuat(Degree angle, float axisX, float axisY, float axisZ) __MathUtil_NOEXCEPT
        : NQuat( static_cast<Radian>(angle), axisX, axisY, axisZ ) {}

#ifdef DXMATH_VEC_UTIL
    NQuat(Radian angle, NVec3 axis) __MathUtil_NOEXCEPT
        : quat_( dx::XMQuaternionRotationAxis(axis.get(), angle) ) {}

    NQuat(Degree angle, NVec3 axis) __MathUtil_NOEXCEPT
        : NQuat( static_cast<Radian>(angle), axis ) {}
#endif  // DXMATH_VEC_UTIL

    NQuat(Radian roll, Radian pitch, Radian yaw) __MathUtil_NOEXCEPT
        : quat_( dx::XMQuaternionRotationRollPitchYaw(pitch, yaw, roll) ) {}

    NQuat(Degree roll, Degree pitch, Degree yaw) __MathUtil_NOEXCEPT
        : NQuat( static_cast<Radian>(roll), static_cast<Radian>(pitch),
            static_cast<Radian>(yaw)
        ) {}

    NQuat(dx::FXMVECTOR quat) __MathUtil_NOEXCEPT
        : quat_(quat) {
        normalize();
    }

    NQuat(Quat quat) __MathUtil_NOEXCEPT
        : quat_(quat.get()) {
        normalize();
    }

    NQuat& XM_CALLCONV operator*=(Quat rhs) __MathUtil_NOEXCEPT {
        return *this *= NQuat(rhs);
    }

    NQuat& XM_CALLCONV operator*=(NQuat rhs) __MathUtil_NOEXCEPT {
        quat_ = dx::XMQuaternionMultiply(quat_, rhs.get());
        return *this;
    }

    NQuat& XM_CALLCONV operator/=(Quat rhs) __MathUtil_NOEXCEPT {
        return *this /= NQuat(rhs);
    }

    NQuat& XM_CALLCONV operator/=(NQuat rhs) __MathUtil_NOEXCEPT {
        quat_ = dx::XMQuaternionMultiply(quat_, dx::XMQuaternionInverse(rhs.get()));
        return *this;
    }

#ifdef DXMATH_MAT_UTIL
    const dx::XMMATRIX XM_CALLCONV mat() const __MathUtil_NOEXCEPT {
        return dx::XMMatrixRotationQuaternion(quat_);
    }

    operator Mat<3, 3>() const __MathUtil_NOEXCEPT;
    operator Mat<4, 4>() const __MathUtil_NOEXCEPT;
#endif  // DXMATH_MAT_UTIL

    operator dx::XMMATRIX() const __MathUtil_NOEXCEPT {
        return dx::XMMatrixRotationQuaternion(quat_);
    }

    dx::XMVECTOR& get() __MathUtil_NOEXCEPT {
        return quat_;
    }

    const dx::XMVECTOR& get() const __MathUtil_NOEXCEPT {
        return quat_;
    }

    const NQuat dual() const __MathUtil_NOEXCEPT {
        return NQuat(dx::XMQuaternionConjugate(quat_), NoNormalize_t());
    }

    const NQuat operator~() const __MathUtil_NOEXCEPT {
        return dual();
    }

    const NQuat operator-() const __MathUtil_NOEXCEPT {
        return NQuat(dx::XMVectorNegate(quat_), NoNormalize_t());
    }

    float XM_CALLCONV x() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetX(quat_);
    }

    float XM_CALLCONV y() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetY(quat_);
    }

    float XM_CALLCONV z() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetZ(quat_);
    }

    float XM_CALLCONV w() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetW(quat_);
    }

    float XM_CALLCONV operator[](std::size_t idx) const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetByIndex(quat_, idx);
    }

    float XM_CALLCONV len() const __MathUtil_NOEXCEPT {
        return 1.f;
    }

    float XM_CALLCONV len2() const __MathUtil_NOEXCEPT {
        return 1.f;
    }

    float XM_CALLCONV norm() const __MathUtil_NOEXCEPT {
        return 1.f;
    }

private:
    // constructor omitting normalization
    NQuat(dx::FXMVECTOR quat, NoNormalize_t) __MathUtil_NOEXCEPT
        : quat_(quat) {}

    void normalize() __MathUtil_NOEXCEPT {
        quat_ = dx::XMQuaternionNormalize(quat_);
    }

    dx::XMVECTOR quat_;

};

inline const Quat XM_CALLCONV operator+(Quat lhs, Quat rhs) __MathUtil_NOEXCEPT {
    return dx::XMVectorAdd(lhs.get(), rhs.get());
}

inline const Quat XM_CALLCONV operator-(Quat lhs, Quat rhs) __MathUtil_NOEXCEPT {
    return dx::XMVectorSubtract(lhs.get(), rhs.get());
}

inline const Quat XM_CALLCONV operator*(Quat lhs, Quat rhs) __MathUtil_NOEXCEPT {
    return dx::XMQuaternionMultiply(lhs.get(), rhs.get());
}

inline const Quat XM_CALLCONV operator/(Quat lhs, Quat rhs) __MathUtil_NOEXCEPT {
    return dx::XMQuaternionMultiply(lhs.get(), dx::XMQuaternionInverse(rhs.get()));
}

inline const Quat XM_CALLCONV quatRPY(
    Radian roll, Radian pitch, Radian yaw
) __MathUtil_NOEXCEPT {
    return dx::XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);
}

inline const Quat XM_CALLCONV quatRotAxis(
    dx::FXMVECTOR axis, Radian angle
) __MathUtil_NOEXCEPT {
    return dx::XMQuaternionRotationAxis(axis, angle);
}

inline const Quat XM_CALLCONV quatRotMat(
    dx::FXMMATRIX mat
) __MathUtil_NOEXCEPT {
    return dx::XMQuaternionRotationMatrix(mat);
}

inline const Quat XM_CALLCONV slerp(
    Quat lhs, Quat rhs, float t
) __MathUtil_NOEXCEPT {
    return dx::XMQuaternionSlerp(lhs.get(), rhs.get(), t);
}

inline const Quat XM_CALLCONV squad(
    Quat q0, Quat q1, Quat a, Quat b, float t
) __MathUtil_NOEXCEPT {
    return dx::XMQuaternionSquad(q0.get(), q1.get(), a.get(), b.get(), t);
}

#endif  // DXMATH_QUAT_UTIL

#if defined(DXMATH_MAT_UTIL) and defined(DXMATH_VEC_UTIL)

template <std::size_t R, std::size_t C>
    requires (R >= 1 && R <= 4) && (C >= 1 && C <= 4)
class alignas(16) Mat {
public:
    template <std::size_t R, std::size_t C>
        requires (R >= 1 && R <= 4) && (C >= 1 && C <= 4)
    friend class Mat;

    Mat() __MathUtil_NOEXCEPT
        : Mat(dx::XMMatrixIdentity()) {}
    
    Mat(float val) __MathUtil_NOEXCEPT
        : Mat(dx::XMMatrixIdentity() * val) {}

    Mat(dx::FXMMATRIX mat) __MathUtil_NOEXCEPT
        : mat_(mat) {}

    template <std::size_t R2, std::size_t C2>
    Mat(const Mat<R2, C2>& other) __MathUtil_NOEXCEPT
        : mat_(other.mat_) {}

#ifdef DXMATH_QUAT_UTIL
    Mat(class NQuat nQuat) __MathUtil_NOEXCEPT
        requires ( (R == 3 && C == 3) || (R == 4 && C == 4) )
        : mat_(dx::XMMatrixRotationQuaternion(nQuat.get())) {}
#endif // DXMATH_QUAT_UTIL

#ifdef ASSIMP_MATH_UTIL
    Mat(const aiMatrix3x3& mat) __MathUtil_NOEXCEPT
        : mat_( dx::XMLoadFloat3x3( reinterpret_cast<const dx::XMFLOAT3X3*>(&mat) ) ) {}

    Mat(const aiMatrix4x4& mat) __MathUtil_NOEXCEPT
        : mat_( dx::XMLoadFloat4x4( reinterpret_cast<const dx::XMFLOAT4X4*>(&mat) ) ) {}

    template <std::floating_point Fl>
    Mat(const aiMatrix3x3t<Fl>& mat) __MathUtil_NOEXCEPT
        : mat_( dx::XMMatrixSet(
            mat.a1, mat.a2, mat.a3, 0.f,
            mat.b1, mat.b2, mat.b3, 0.f,
            mat.c1, mat.c2, mat.c3, 0.f,
            0.f, 0.f, 0.f, 1.f
        ) ) {}

    template <std::floating_point Fl>
    Mat(const aiMatrix4x4t<Fl>& mat) __MathUtil_NOEXCEPT
        : mat_( dx::XMMatrixSet(
            mat.a1, mat.a2, mat.a3, mat.a4,
            mat.b1, mat.b2, mat.b3, mat.b4,
            mat.c1, mat.c2, mat.c3, mat.c4,
            mat.d1, mat.d2, mat.d3, mat.d4
        ) ) {}
#endif  // ASSIMP_MATH_UTIL

    Mat& XM_CALLCONV operator+=(Mat rhs) __MathUtil_NOEXCEPT {
        using dx::operator+=;
        mat_ += rhs.mat_;
        return *this;
    }

    Mat& XM_CALLCONV operator-=(Mat rhs) __MathUtil_NOEXCEPT {
        using dx::operator-=;
        mat_ -= rhs.mat_;
        return *this;
    }

    Mat& XM_CALLCONV operator*=(Mat rhs) __MathUtil_NOEXCEPT {
        mat_ = dx::XMMatrixMultiply(mat_, rhs.mat_);
        return *this;
    }

    Mat& XM_CALLCONV operator/=(Mat rhs) __MathUtil_NOEXCEPT {
        mat_ = dx::XMMatrixMultiply(mat_, dx::XMMatrixInverse(nullptr, rhs.mat_));
        return *this;
    }
    
#ifdef _XM_NO_INTRINSICS_
    float XM_CALLCONV operator()(std::size_t r, std::size_t c) const __MathUtil_NOEXCEPT {
        return mat_(r, c)
    }
#else
    float XM_CALLCONV operator()(std::size_t r, std::size_t c) const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetByIndex(mat_.r[r], c);
    }
#endif

    const Vec<R> XM_CALLCONV row(std::size_t r) const __MathUtil_NOEXCEPT {
        return Vec<R>(mat_.r[r]);
    }

    const Vec<C> XM_CALLCONV col(std::size_t c) const __MathUtil_NOEXCEPT {
        return Vec<C>(
            dx::XMVectorGetByIndex(mat_.r[0], c),
            dx::XMVectorGetByIndex(mat_.r[1], c),
            dx::XMVectorGetByIndex(mat_.r[2], c),
            dx::XMVectorGetByIndex(mat_.r[3], c)
        );
    }

    float XM_CALLCONV det() const __MathUtil_NOEXCEPT {
        return dx::XMVectorGetX(dx::XMMatrixDeterminant(mat_));
    }

    dx::XMMATRIX& get() __MathUtil_NOEXCEPT {
        return mat_;
    }

    const dx::XMMATRIX& get() const __MathUtil_NOEXCEPT {
        return mat_;
    }

    const Mat XM_CALLCONV operator-() const __MathUtil_NOEXCEPT {
        return dx::XMMatrixMultiply( mat_, dx::XMMatrixScalingFromVector(
            dx::XMVectorNegate( dx::XMVectorSplatOne() )
        ) );
    }

private:
    dx::XMMATRIX mat_;
};

#if defined(DXMATH_QUAT_UTIL) && defined(DXMATH_MAT_UTIL)
inline NQuat::operator Mat<3, 3>() const __MathUtil_NOEXCEPT {
    return Mat<3, 3>(dx::XMMatrixRotationQuaternion(quat_));
}

inline NQuat::operator Mat<4, 4>() const __MathUtil_NOEXCEPT {
    return Mat<4, 4>(dx::XMMatrixRotationQuaternion(quat_));
}
#endif  // DXMATH_QUAT_UTIL && DXMATH_MAT_UTIL

template <std::size_t R, std::size_t C>
const Mat<R, C> XM_CALLCONV operator+(Mat<R, C> lhs, Mat<R, C> rhs) __MathUtil_NOEXCEPT {
    return lhs.get() + rhs.get();
}

template <std::size_t R, std::size_t C>
const Mat<R, C> XM_CALLCONV operator-(Mat<R, C> lhs, Mat<R, C> rhs) __MathUtil_NOEXCEPT {
    return lhs.get() - rhs.get();
}

template <std::size_t R, std::size_t C>
const Mat<R, C> XM_CALLCONV operator*(Mat<R, C> lhs, Mat<R, C> rhs) __MathUtil_NOEXCEPT {
    return dx::XMMatrixMultiply(lhs.get(), rhs.get());
}

template <std::size_t R, std::size_t C>
const Mat<R, C> XM_CALLCONV operator/(Mat<R, C> lhs, Mat<R, C> rhs) __MathUtil_NOEXCEPT {
    return dx::XMMatrixMultiply(lhs.get(), dx::XMMatrixInverse(nullptr, rhs.get()));
}

template <std::size_t R, std::size_t C>
const Vec<C> XM_CALLCONV operator*(Vec<R> lhs, Mat<R, C> rhs) __MathUtil_NOEXCEPT {
    return dx::XMVector3Transform(lhs.get(), rhs.get());
}

template <std::size_t D>
    requires (D >= 1 && D <= 4)
Vec<D>& XM_CALLCONV Vec<D>::operator*=(Mat<D, D> rhs) __MathUtil_NOEXCEPT {
    vec_ = dx::XMVector3Transform(vec_, rhs.get());
    return *this;
}

inline const Mat<3, 3> XM_CALLCONV scale(float xScl, float yScl, float zScl) __MathUtil_NOEXCEPT {
    return dx::XMMatrixScaling(xScl, yScl, zScl);
}

inline const Mat<3, 3> XM_CALLCONV scale(Vec<3> vec) __MathUtil_NOEXCEPT {
    return dx::XMMatrixScalingFromVector(vec.get());
}

inline const Mat<4, 4> XM_CALLCONV scaleH(Vec<3> vec) __MathUtil_NOEXCEPT {
    return dx::XMMatrixScalingFromVector(vec.get());
}

inline const Mat<4, 4> XM_CALLCONV translate(float x, float y, float z) __MathUtil_NOEXCEPT {
    return dx::XMMatrixTranslation(x, y, z);
}

inline const Mat<4, 4> XM_CALLCONV translate(Vec<3> vec) __MathUtil_NOEXCEPT {
    return dx::XMMatrixTranslationFromVector(vec.get());
}

inline const Mat<3, 3> XM_CALLCONV rotate(Radian angle, float axisX, float axisY, float axisZ) __MathUtil_NOEXCEPT {
    return dx::XMMatrixRotationAxis(dx::XMVectorSet(axisX, axisY, axisZ, 0.f), angle);
}

inline const Mat<3, 3> XM_CALLCONV rotate(Radian angle, Vec<3> axis) __MathUtil_NOEXCEPT {
    return dx::XMMatrixRotationAxis(axis.get(), angle);
}

inline const Mat<3, 3> XM_CALLCONV rotate(Radian angle, NVec<3> axis) __MathUtil_NOEXCEPT {
    return dx::XMMatrixRotationNormal(axis.get(), angle);
}

inline const Mat<3, 3> XM_CALLCONV rotate(Degree angle, float axisX, float axisY, float axisZ) __MathUtil_NOEXCEPT {
    return rotate(static_cast<Radian>(angle), axisX, axisY, axisZ);
}

inline const Mat<3, 3> XM_CALLCONV rotate(Degree angle, Vec<3> axis) __MathUtil_NOEXCEPT {
    return rotate(static_cast<Radian>(angle), axis);
}

inline const Mat<3, 3> XM_CALLCONV rotate(Degree angle, NVec<3> axis) __MathUtil_NOEXCEPT {
    return rotate(static_cast<Radian>(angle), axis);
}

inline const Mat<3, 3> XM_CALLCONV rotateX(Radian angle) __MathUtil_NOEXCEPT {
    return dx::XMMatrixRotationX(angle);
}

inline const Mat<3, 3> XM_CALLCONV rotateX(Degree angle) __MathUtil_NOEXCEPT {
    return rotateX(static_cast<Radian>(angle));
}

inline const Mat<3, 3> XM_CALLCONV rotateY(Radian angle) __MathUtil_NOEXCEPT {
    return dx::XMMatrixRotationY(angle);
}

inline const Mat<3, 3> XM_CALLCONV rotateY(Degree angle) __MathUtil_NOEXCEPT {
    return rotateY(static_cast<Radian>(angle));
}

inline const Mat<3, 3> XM_CALLCONV rotateZ(Radian angle) __MathUtil_NOEXCEPT {
    return dx::XMMatrixRotationZ(angle);
}

inline const Mat<3, 3> XM_CALLCONV rotateZ(Degree angle) __MathUtil_NOEXCEPT {
    return rotateZ(static_cast<Radian>(angle));
}

inline const Mat<3, 3> XM_CALLCONV rotateRPY(Radian roll, Radian pitch, Radian yaw) __MathUtil_NOEXCEPT {
    return dx::XMMatrixRotationRollPitchYaw(pitch, yaw, roll);
}

inline const Mat<3, 3> XM_CALLCONV rotateRPY(Degree roll, Degree pitch, Degree yaw) __MathUtil_NOEXCEPT {
    return rotateRPY(
        static_cast<Radian>(roll),
        static_cast<Radian>(pitch),
        static_cast<Radian>(yaw)
    );
}

inline const Mat<4, 4> XM_CALLCONV rotateH(Radian angle, float axisX, float axisY, float axisZ) __MathUtil_NOEXCEPT {
    return dx::XMMatrixRotationAxis(dx::XMVectorSet(axisX, axisY, axisZ, 0.f), angle);
}

inline const Mat<4, 4> XM_CALLCONV rotateH(Radian angle, Vec<3> axis) __MathUtil_NOEXCEPT {
    return dx::XMMatrixRotationAxis(axis.get(), angle);
}

inline const Mat<4, 4> XM_CALLCONV rotateH(Radian angle, NVec<3> axis) __MathUtil_NOEXCEPT {
    return dx::XMMatrixRotationNormal(axis.get(), angle);
}

inline const Mat<4, 4> XM_CALLCONV rotateH(Degree angle, float axisX, float axisY, float axisZ) __MathUtil_NOEXCEPT {
    return rotateH(static_cast<Radian>(angle), axisX, axisY, axisZ);
}

inline const Mat<4, 4> XM_CALLCONV rotateH(Degree angle, Vec<3> axis) __MathUtil_NOEXCEPT {
    return rotateH(static_cast<Radian>(angle), axis);
}

inline const Mat<4, 4> XM_CALLCONV rotateH(Degree angle, NVec<3> axis) __MathUtil_NOEXCEPT {
    return rotateH(static_cast<Radian>(angle), axis);
}

inline const Mat<4, 4> XM_CALLCONV rotateXH(Radian angle) __MathUtil_NOEXCEPT {
    return dx::XMMatrixRotationX(angle);
}

inline const Mat<4, 4> XM_CALLCONV rotateXH(Degree angle) __MathUtil_NOEXCEPT {
    return rotateXH(static_cast<Radian>(angle));
}

inline const Mat<4, 4> XM_CALLCONV rotateYH(Radian angle) __MathUtil_NOEXCEPT {
    return dx::XMMatrixRotationY(angle);
}

inline const Mat<4, 4> XM_CALLCONV rotateYH(Degree angle) __MathUtil_NOEXCEPT {
    return rotateYH(static_cast<Radian>(angle));
}

inline const Mat<4, 4> XM_CALLCONV rotateZH(Radian angle) __MathUtil_NOEXCEPT {
    return dx::XMMatrixRotationZ(angle);
}

inline const Mat<4, 4> XM_CALLCONV rotateZH(Degree angle) __MathUtil_NOEXCEPT {
    return rotateZH(static_cast<Radian>(angle));
}

inline const Mat<4, 4> XM_CALLCONV rotateRPYH(Radian roll, Radian pitch, Radian yaw) __MathUtil_NOEXCEPT {
    return dx::XMMatrixRotationRollPitchYaw(pitch, yaw, roll);
}

inline const Mat<3, 3> XM_CALLCONV transpose(Mat<3, 3> mat) __MathUtil_NOEXCEPT {
    return dx::XMMatrixTranspose(mat.get());
}

inline const Mat<4, 4> XM_CALLCONV transpose(Mat<4, 4> mat) __MathUtil_NOEXCEPT {
    return dx::XMMatrixTranspose(mat.get());
}

inline const Mat<3, 3> XM_CALLCONV inverse(Mat<3, 3> mat) __MathUtil_NOEXCEPT {
    return dx::XMMatrixInverse(nullptr, mat.get());
}

inline const Mat<3, 3> XM_CALLCONV inverse(Mat<3, 3> mat, float& outDet) __MathUtil_NOEXCEPT {
    dx::XMVECTOR det;
    auto ret = dx::XMMatrixInverse(&det, mat.get());
    outDet = dx::XMVectorGetX(det);
    return ret;
}

inline const Mat<4, 4> XM_CALLCONV inverse(Mat<4, 4> mat) __MathUtil_NOEXCEPT {
    return dx::XMMatrixInverse(nullptr, mat.get());
}

inline const Mat<4, 4> XM_CALLCONV inverse(Mat<4, 4> mat, float& outDet) __MathUtil_NOEXCEPT {
    dx::XMVECTOR det;
    auto ret = dx::XMMatrixInverse(&det, mat.get());
    outDet = dx::XMVectorGetX(det);
    return ret;
}

inline const Mat<4, 4> XM_CALLCONV lookAt(
    Vec<3> eye, Vec<3> at, NVec<3> up
) __MathUtil_NOEXCEPT {
    return dx::XMMatrixLookAtLH(eye.get(), at.get(), up.get());
}

inline const Mat<4, 4> XM_CALLCONV ortho(
    float width, float height, float nearZ, float farZ
) __MathUtil_NOEXCEPT {
    return dx::XMMatrixOrthographicLH(width, height, nearZ, farZ);
}

inline const Mat<4, 4> XM_CALLCONV ortho(
    float left, float right, float bottom, float top, float nearZ, float farZ
) __MathUtil_NOEXCEPT {
    return dx::XMMatrixOrthographicOffCenterLH(left, right, bottom, top, nearZ, farZ);
}

inline const Mat<4, 4> XM_CALLCONV persp(
    float width, float height, float nearZ, float farZ
) __MathUtil_NOEXCEPT {
    return dx::XMMatrixPerspectiveLH(width, height, nearZ, farZ);
}

inline const Mat<4, 4> XM_CALLCONV persp(
    Radian fov, float aspect, float nearZ, float farZ
) __MathUtil_NOEXCEPT {
    return dx::XMMatrixPerspectiveFovLH(fov, aspect, nearZ, farZ);
}

inline const Mat<4, 4> XM_CALLCONV persp(
    Degree fov, float aspect, float nearZ, float farZ
) __MathUtil_NOEXCEPT {
    return persp(static_cast<Radian>(fov), aspect, nearZ, farZ);
}

inline const Mat<4, 4> XM_CALLCONV persp(
    float left, float right, float bottom, float top, float nearZ, float farZ
) __MathUtil_NOEXCEPT {
    return dx::XMMatrixPerspectiveOffCenterLH(left, right, bottom, top, nearZ, farZ);
}

using Mat1x1 = Mat<1, 1>;
using Mat1x2 = Mat<1, 2>;
using Mat1x3 = Mat<1, 3>;
using Mat1x4 = Mat<1, 4>;
using Mat2x1 = Mat<2, 1>;
using Mat2x2 = Mat<2, 2>;
using Mat2x3 = Mat<2, 3>;
using Mat2x4 = Mat<2, 4>;
using Mat3x1 = Mat<3, 1>;
using Mat3x2 = Mat<3, 2>;
using Mat3x3 = Mat<3, 3>;
using Mat3x4 = Mat<3, 4>;
using Mat4x1 = Mat<4, 1>;
using Mat4x2 = Mat<4, 2>;
using Mat4x3 = Mat<4, 3>;
using Mat4x4 = Mat<4, 4>;

#endif  // DXMATH_MAT_UTIL

}   // namespace mu

#endif  // __MathUtil_HPP