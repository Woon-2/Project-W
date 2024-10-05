#ifndef __Renderer_HPP
#define __Renderer_HPP

#include "gfx.hpp"
#include "renderProtocol.hpp"

#include "shader.hpp"

#include <vector>
#include <ranges>
#include <algorithm>

#include <cassert>

namespace gfx {

template <class TShader>
class Renderer : public IRenderer {
public:
    void pushShader(rp::Protocol protocol, const TShader* pShader) {
        assert(pShader != nullptr);
        assert(pShader->supports(protocol));
        pairs_.emplace_back(protocol, pShader);
    }

    bool supports(rp::Protocol protocol) const {
        return std::ranges::any_of(pairs_, [protocol](const auto& pair) {
            return pair.first == protocol;
        } );
    }

    void render( const class IScene& scene, class IRenderContext& renderContext,
        class IRenderTarget& target
    ) const override {
        for (const auto& pair : pairs_) {
            auto protocol = pair.first;
            auto pShader = pair.second;

            /* pShader->bind(renderContext, protocol::option); */
            pShader->bind(renderContext, std::any{});
            pShader->draw(renderContext, scene, target, protocol);
        }
    }

    void cleanup() override {
        pairs_.clear();
    }

private:
    std::vector< std::pair<rp::Protocol, const TShader*> > pairs_;
};

}   // namespace gfx

#endif  // __Renderer_HPP