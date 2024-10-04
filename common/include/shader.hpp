#ifndef __Shader_HPP
#define __Shader_HPP

#include "gfx.hpp"

#include "renderProtocol.hpp"
#include "inputLayout.hpp"

#include <vector>
#include <ranges>
#include <algorithm>
#include <type_traits>
#include <any>

namespace gfx {

template <class IL>
class Shader {
protected:
    std::vector<IL> inputLayouts_;
    std::vector<rp::Protocol> protocols_;
    InputLayout::VBFlags vbFlags_;

public:
    using ILIdx = std::size_t;

    void pushProtocol(rp::Protocol protocol) {
        protocols_.push_back(protocol);
    }

    ILIdx pushInputLayout(const IL& il) {
        inputLayouts_.push_back(il);
        vbFlags_.push_back(il.flags());
        return inputLayouts_.size() - 1;
    }

    ILIdx pushInputLayout(IL&& il) {
        inputLayouts_.push_back(std::move(il));
        vbFlags_.push_back(inputLayouts_.back().flags());
        return inputLayouts_.size() - 1;
    }

    template <std::ranges::range R>
    ILIdx pushInputLayouts(R&& ils) {
        auto idx = inputLayouts_.size();

        if constexpr (std::ranges::sized_range<R>) {
            inputLayouts_.reserve(inputLayouts_.size() + ils.size());
        }
        if constexpr (std::is_lvalue_reference_v< std::remove_const_t<R> >) {
            std::ranges::copy(ils, std::back_inserter(inputLayouts_));
            std::ranges::transform( ils, std::back_inserter(vbFlags_), [](const auto& il) {
                return il.flags();
            } );
        } else {
            // must call transform first, otherwise it will copy invalid flags from moved-from objects.
            std::ranges::transform( ils, std::back_inserter(vbFlags_), [](const auto& il) {
                return il.flags();
            } );
            std::ranges::move(ils, std::back_inserter(inputLayouts_));
        }

        return idx;
    }

    InputLayout::VBFlags& vbFlags() NOEXCEPT {
        return vbFlags_;
    }

    const InputLayout::VBFlags& vbFlags() const NOEXCEPT {
        return vbFlags_;
    }

    bool supports(rp::Protocol protocol) const {
        return std::ranges::find(protocols_, protocol) != protocols_.end();
    }

    virtual void bind(IRenderContext& ctx, std::any option) const = 0;
    virtual void draw(IRenderContext& ctx, const IScene& scene) const = 0;
};

}   // namespace gfx

#endif  // __Shader_HPP