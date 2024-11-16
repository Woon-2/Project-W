#ifndef __DxMathUtil_HPP
#define __DxMathUtil_HPP

#include <DirectXMath.h>

#include <type_traits>

namespace DirectX {

template <class T>
T XM_CALLCONV convertMat(FXMMATRIX mat) {
    T result;
    if constexpr (std::is_same_v<T, XMFLOAT4X4>) {
        dx::XMStoreFloat4x4(&result, mat);
    } else if constexpr (std::is_same_v<T, XMFLOAT3X3>) {
        dx::XMStoreFloat3x3(&result, mat);
    } else if constexpr (std::is_same_v<T, XMFLOAT3X4>) {
        dx::XMStoreFloat3x4(&result, mat);
    }
    return result;
}

}   // namespace DirectX

namespace dx = DirectX;

#endif // __DxMathUtil_HPP