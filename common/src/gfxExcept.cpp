#include "gfxExcept.hpp"

namespace gfx {
    namespace {
        constexpr auto incompatibleRenderContextDesc
            = "[Description] Tried to create incompatible render context from the given graphics core.\n";
    }

    IncompatibleRenderContext::IncompatibleRenderContext(
        int lineNum, const char* fileStr
    ) NOEXCEPT
        : Exception( lineNum, fileStr, incompatibleRenderContextDesc ) {}
}   // namespace gfx