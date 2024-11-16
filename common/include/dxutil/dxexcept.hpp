#ifndef __DXEXCEPT_HPP
#define __DXEXCEPT_HPP

#include "gfxExcept.hpp"
#include "dxutil/dxtarget.hpp"

/**
 * @page page1 Conditional Compilation
 * @brief Set of macros for conditional compilation.
 * 
 * - @ref ENABLE_DXGI_INFO "ENABLE_DXGI_INFO"
 */

/**
 * @def ENABLE_DXGI_INFO
 * Enables usage of DXGI info queue for logging warnings and errors.
 */
#define ENABLE_DXGI_INFO

using namespace std::literals;

#ifdef ENABLE_DXGI_INFO
#include <dxgidebug.h>

#include <iostream>
#include <sstream>
#endif  // ENABLE_DXGI_INFO

#include <system_error>

#include "config.hpp"

/**
 * @file dxexcept.hpp
 */

/**
 * @page page2 Exceptions
 * @brief Custom exceptions
 * 
 * - @ref DX_EXCEPT(hr) "DX_EXCEPT(hr)"
 * - @ref DX_EXCEPT_VOID() "DX_EXCEPT_VOID()"
 * - @ref DX_THROW_FAILED(hrcall) "DX_THROW_FAILED(hrcall)"
 * - @ref DX_THROW_FAILED_VOID(voidcall) "DX_THROW_FAILED_VOID(voidcall)"
 */

/**
 * @def DX_EXCEPT(hr)
 * Constructs a gfx::DXException with the given HRESULT.    
 * The line number and file name of it's call site are automatically included in the exception.
 */
#define DX_EXCEPT(hr) gfx::DXException(__LINE__, __FILE__, hr)
/**
 * @def DX_EXCEPT_VOID()
 * Constructs a gfx::DXException with E_INVALIDARG.    
 * The line number and file name of it's call site are automatically included in the exception.
 */
#define DX_EXCEPT_VOID() gfx::DXException(__LINE__, __FILE__, E_INVALIDARG)

/**
 * @def DX_THROW_FAILED(hrcall)
 * Throws a gfx::DXException if the HRESULT of hrcall is less than 0.
 */
#define DX_THROW_FAILED(hrcall) \
    if (auto hr = (hrcall); hr < 0) { \
        throw DX_EXCEPT(hr); \
    }

#ifdef ENABLE_DXGI_INFO
/**
 * @def DX_THROW_FAILED_VOID(voidcall)
 * Throws a gfx::DXException if DXGI info warnings or errors are logged during voidcall.    
 * If ENABLE_DXGI_INFO is not defined, voidcall is executed without any checks.
 */
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

/**
 * @brief Wrapper for DXGI info queue.    
 * It is used to log warnings and errors from the DXGI debug layer for exception handling.    
 * It is only available when ENABLE_DXGI_INFO is defined.
 * 
 * As the logging-related features are automatically used by exception handling macros,    
 * All you need to do is initializing it and cleaning it up at the start and end of your program.    
 * If you want to manually get the logged messages, you can use the dump method.    
 * 
 * Since the DXGI info queue is a global object, it is not thread-safe, and it has only static methods.
 * 
 * @note DXInfoQueue::init must be called before any DXGI objects are created,     
 * indicating that it has to precede even the creation of the ICore implementation or DXGI Factory.
 * 
 */
class DXInfoQueue {
public:
    DXInfoQueue() = delete;
    DXInfoQueue(const DXInfoQueue&) = delete;
    DXInfoQueue(DXInfoQueue&&) noexcept = delete;
    DXInfoQueue& operator=(const DXInfoQueue&) = delete;
    DXInfoQueue& operator=(DXInfoQueue&&) noexcept = delete;

    /**
     * @brief Initializes the DXGI info queue.
     * @throw DXInfoQException if the initialization fails.
     */
    static void init();
    /**
     * @brief Dumps the logged messages to the given output stream.
     * @param os The output stream to write the messages.
     */
    static void dump(std::ostream& os);
    /**
     * @brief Cleans up the DXGI info queue.    
     * It reports any remaining resources that are not released which can be considered as memory leaks.
     */
    static void cleanup();

    /**
     * @brief Gets the number of stored messages in the DXGI info queue.
     * @param category The category of the messages to count.
     * @return The number of stored messages.
     */
    static std::size_t size() NOEXCEPT {
        return spInfoQ->GetNumStoredMessages(DXGI_DEBUG_ALL);
    }

    /**
     * @brief Checks if the DXGI info queue is empty.
     * @return true if the queue is empty, false otherwise.
     */
    static bool empty() NOEXCEPT {
        return size() == 0u;
    }

private:
    static wrl::ComPtr<IDXGIInfoQueue> spInfoQ;
    static wrl::ComPtr<IDXGIDebug1> spDebug;
};

/**
 * @brief Exception for DXGI info queue.     
 * It is thrown when the DXGI info queue's internal funcionailities fails.
 */
class DXInfoQException : public Exception {
public:
    using Exception::Exception;

    const char* type() const NOEXCEPT override {
        return "DXInfoQueue Exception";
    }
};

namespace detail {

std::string makeHRDesc(HRESULT hr);

}   // namespace gfx::detail

/**
 * @brief Exception for DXGI errors.    
 * It is thrown when a DXGI function fails.     
 * To get detailed information from DXGI about the error, ENABLE_DXGI_INFO must be defined.
 * Otherwise, only the `HRESULT` and description of the `HRESULT` is included in the exception message.
 */
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