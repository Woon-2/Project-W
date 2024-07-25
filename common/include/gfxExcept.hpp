#ifndef __GFXEXCEPT_HPP
#define __GFXEXCEPT_HPP

#include <string>
#include <string_view>

#include "Woon2Exception.hpp"

#define GFX_EXCEPT(desc) gfx::Exception(__LINE__, __FILE__, desc)
#define GFX_INCOMPATIBLE_RC() gfx::IncompatibleRenderContext(__LINE__, __FILE__)

namespace gfx {
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

    class IncompatibleRenderContext : public Exception {
    public:
        IncompatibleRenderContext( int lineNum, const char* fileStr ) NOEXCEPT;
    };
}   // namespace gfx

#endif  // __GFXEXCEPT_HPP