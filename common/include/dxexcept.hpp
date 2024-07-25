#ifndef __DXEXCEPT_HPP
#define __DXEXCEPT_HPP

#include "gfxExcept.hpp"
#include "dxtarget.hpp"

#define ENABLE_DXGI_INFO

using namespace std::literals;

#ifdef ENABLE_DXGI_INFO
#include <dxgidebug.h>

#include <iostream>
#include <sstream>
#endif  // ENABLE_DXGI_INFO

#include <system_error>


#define DX_EXCEPT(hr) gfx::DXException(__LINE__, __FILE__, hr)
#define DX_EXCEPT_VOID() gfx::DXException(__LINE__, __FILE__, E_INVALIDARG)

#define DX_THROW_FAILED(hrcall) \
    if (auto hr = (hrcall); hr < 0) { \
        throw DX_EXCEPT(hr); \
    }

#ifdef ENABLE_DXGI_INFO
#define DX_THROW_FAILED_VOID(voidcall)  \
    [&]() { \
        auto __LoggedMessageSize = gfx::DXInfoQueue::size(); \
        (voidcall); \
        if ( gfx::DXInfoQueue::size() != __LoggedMessageSize ) { \
            throw DX_EXCEPT_VOID(); \
        } \
    }()
#else   // ENABLE_DXGI_INFO
#define DX_THROW_FAILED_VOID(voidcall) (voidcall)
#endif  // ENABLE_DXGI_INFO

namespace gfx {

#ifdef ENABLE_DXGI_INFO

class DXInfoQueue {
public:
    DXInfoQueue() = delete;
    DXInfoQueue(const DXInfoQueue&) = delete;
    DXInfoQueue(DXInfoQueue&&) noexcept = delete;
    DXInfoQueue& operator=(const DXInfoQueue&) = delete;
    DXInfoQueue& operator=(DXInfoQueue&&) noexcept = delete;

    static void init();
    static void dump(std::ostream& os);
    static void cleanup();

    static std::size_t size() NOEXCEPT {
        return pInfoQ->GetNumStoredMessages(DXGI_DEBUG_ALL);
    }

    static bool empty() NOEXCEPT {
        return size() == 0u;
    }

private:
    static wrl::ComPtr<IDXGIInfoQueue> pInfoQ;
    static wrl::ComPtr<IDXGIDebug1> pDebug;
};

wrl::ComPtr<IDXGIInfoQueue> DXInfoQueue::pInfoQ;
wrl::ComPtr<IDXGIDebug1> DXInfoQueue::pDebug;

class DXInfoQException : public Exception {
public:
    using Exception::Exception;

    const char* type() const NOEXCEPT override {
        return "DXInfoQueue Exception";
    }
};

namespace detail {

std::string makeHRDesc(HRESULT hr) {
    return "[Error Code] " + std::to_string(hr) + "\n"
        + "[Description] " + std::system_category().message(hr) + "\n";
}

}   // namespace gfx::detail

class DXException : public Exception {
public:
    DXException( int lineNum, const char* fileStr, HRESULT hr ) NOEXCEPT
        : Exception( lineNum, fileStr, detail::makeHRDesc(hr) ) {
        auto oss = std::ostringstream();
        DXInfoQueue::dump(oss);
        whatBuffer_ += "\n[Details] " + oss.str() + "\n";
    }

    const char* type() const NOEXCEPT override {
        return "DirectX Exception";
    }
};

#else   // ENABLE_DXGI_INFO

class DXException : public Exception {
public:
    DXException( int lineNum, const char* fileStr, HRESULT hr ) NOEXCEPT
        : Exception( lineNum, fileStr, detail::makeHRDesc(hr) ) {}

    const char* type() const NOEXCEPT override {
        return "DirectX Exception";
    }
};
#endif  // ENABLE_DXGI_INFO

}   // namespace gfx

#endif  // __DXEXCEPT_HPP