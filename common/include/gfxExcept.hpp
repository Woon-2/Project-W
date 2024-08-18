#ifndef __GFXEXCEPT_HPP
#define __GFXEXCEPT_HPP

#include <string>
#include <string_view>

#include "Woon2Exception.hpp"

#include "config.hpp"

/**
 * @file gfxExcept.hpp
 */

/**
 * @page page2 Exceptions
 * @brief Custom exceptions
 * 
 * - @ref GFX_EXCEPT(desc) "GFX_EXCEPT(desc)"
 * - @ref GFX_INCOMPATIBLE_RC() "GFX_INCOMPATIBLE_RC()"
 */

/**
 * @def GFX_EXCEPT(desc)
 * Constructs a gfx::Exception with the given description.    
 * The line number and file name of it's call site are automatically included in the exception.
 */
#define GFX_EXCEPT(desc) gfx::Exception(__LINE__, __FILE__, desc)
/**
 * @def GFX_INCOMPATIBLE_RC()
 * Constructs a gfx::IncompatibleRenderContext exception.
 * The line number and file name of it's call site are automatically included in the exception.
 */
#define GFX_INCOMPATIBLE_RC() gfx::IncompatibleRenderContext(__LINE__, __FILE__)

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
    class Exception : public Woon2Exception {
    public:
        Exception( int lineNum, const char* fileStr, std::string_view desc ) NOEXCEPT
            : Woon2Exception( lineNum, fileStr ) {
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