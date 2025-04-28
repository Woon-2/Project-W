#ifndef __GFXEXCEPT_HPP
#define __GFXEXCEPT_HPP

#include <string>
#include <string_view>

#include "ShException.hpp"

#include "config.hpp"

/**
 * @file gfxExcept.hpp
 */

/**
 * @page page2 Exceptions
 * @brief Custom exceptions
 * 
 * - @ref GFX_EXCEPT(desc) "GFX_EXCEPT(desc)"
 * - @ref GFX_EXCEPT_CLASS(exceptionClsName, ...) "GFX_EXCEPT_CLASS(exceptionClsName, ...)"
 */

/**
 * @def GFX_EXCEPT(desc)
 * Constructs a gfx::Exception with the given description.    
 * The line number and file name of it's call site are automatically included in the exception.
 */
#define GFX_EXCEPT(desc) gfx::Exception(__LINE__, __FILE__, desc)
/**
 * @def GFX_EXCEPT_CLASS(exceptionClsName, ...)
 * Constructs a specific exception class with the meta information(line, file) and varadic arguments.    
 * The macro supposes the exception class to be in the gfx namespace.    
 * If the exception class is in a nested namespace or class,     
 * prepending the namespace or class name except for the gfx namespace is required.
 */
#define GFX_EXCEPT_CLASS(exceptionClsName, ...) gfx::exceptionClsName(__LINE__, __FILE__, __VA_ARGS__)

namespace gfx {
    /**
     * @brief Exception class for graphics functions.
     * @details call Exception::what to get error message, Exception::type to get exception type.
     * @code
     * try {
     *     // some graphics function call
     * } catch (const gfx::Exception& e) {
     *     MessageBoxA(nullptr, e.what(), e.type(), MB_OK | MB_ICONEXCLAMATION);
     * }
     * @endcode
     */
    class Exception : public ShException {
    public:
        Exception( int lineNum, const char* fileStr, std::string_view desc ) NOEXCEPT
            : ShException( lineNum, fileStr ) {
            whatBuffer_ += desc;
        }

        const char* type() const NOEXCEPT override {
            return "Graphics Exception";
        }
    };

    /**
     * @brief Exception for incompatible render contexts.
     */
    class IncompatibleRenderContext : public Exception {
    public:
        IncompatibleRenderContext( int lineNum, const char* fileStr ) NOEXCEPT;
    };
}   // namespace gfx

#endif  // __GFXEXCEPT_HPP