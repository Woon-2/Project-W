#ifndef __Renderer_HPP
#define __Renderer_HPP

#include "gfx.hpp"
#include "renderProtocol.hpp"

#include "shader.hpp"

#include <vector>
#include <ranges>
#include <algorithm>
#include <tuple>
#include <any>

#include <cassert>

namespace gfx {

template <class TShader>
class Renderer : public IRenderer {
public:
    void init(gfx::ICore& core) override {}

    void pushShader(rp::Protocol protocol, TShader* pShader, std::any option = {}) {
        assert(pShader != nullptr);
        assert(pShader->supports(protocol));
        tuples_.emplace_back(protocol, pShader, option);
    }

    bool supports(rp::Protocol protocol) const {
        return std::ranges::any_of(tuples_, [protocol](const auto& pair) {
            return pair.first == protocol;
        } );
    }

    void render( const class IScene& scene, class IRenderContext& renderContext,
        class IRenderTarget& target
    ) const override {
        for (const auto& tuple : tuples_) {
            auto protocol = std::get<rp::Protocol>(tuple);
            auto pShader = std::get<TShader*>(tuple);
            auto option = std::get<std::any>(tuple);

            pShader->bind(renderContext, option);
            pShader->draw(renderContext, scene, target, protocol);
        }
    }

    void cleanup() override {
        tuples_.clear();
    }

private:
    std::vector< std::tuple<rp::Protocol, TShader*, std::any> > tuples_;
};

}   // namespace gfx

#endif  // __Renderer_HPP