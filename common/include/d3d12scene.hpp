#ifndef __D3D12_SCENE_HPP
#define __D3D12_SCENE_HPP

#include "camera.hpp"
#include "d3d12core.hpp"
#include "renderProtocol.hpp"

#include "d3d12model.hpp"
#include "d3d12material.hpp"

#include "mathUtil.hpp"
#include "shaderRes.hpp"
#include "drawInfo.hpp"
#include "gfxExcept.hpp"

#include "generator.hpp"

#include <vector>
#include <ranges>
#include <span>
#include <map>
#include <any>
#include <type_traits>

namespace gfx {

namespace d3d12 {

class CameraScene : public gfx::CameraScene {
public:
    template <class T>
    using Cont = std::vector<T>;

    CameraScene(const Camera& camera)
        : gfx::CameraScene(camera), fragmentsMap_(), lightsMap_() {}

    Generator<DrawInfo> iteration(rp::Protocol protocol) const override;

    void addFragment(Fragment&& fragment) {
        if (!fragment.pMesh->hasProtocol()) {
            throw GFX_EXCEPT("Mesh has no protocol");
        }
        fragmentsMap_[fragment.pMesh->protocol()].push_back(std::move(fragment));
    }

    template <std::ranges::range R>
    void addFragments(rp::Protocol protocol, R&& fragments) {
        /*
        for (const auto& fragment : fragments) {
            assert(fragment.pMesh->protocol() == protocol);
        }
        */

        if constexpr (std::ranges::sized_range<R>) {
            auto& cont = fragmentsMap_[protocol];
            cont.reserve(std::ranges::size(fragments) + cont.size());
        }

        if constexpr (std::is_lvalue_reference_v< std::remove_cv_t<R> >) {
            fragmentsMap_[protocol].insert( std::end(fragmentsMap_[protocol]),
                std::begin(fragments), std::end(fragments)
            );
        } else {
            fragmentsMap_[protocol].insert( std::end(fragmentsMap_[protocol]),
                std::move_iterator(std::begin(fragments)),
                std::move_iterator(std::end(fragments))
            );
        }
    }

    void setFragments(rp::Protocol protocol, Cont<Fragment>&& fragments) {
        /*
        for (const auto& fragment : fragments) {
            assert(fragment.pMesh->protocol() == protocol);
        }
        */
        fragmentsMap_[protocol] = std::move(fragments);
    }

    void addLight(rp::Protocol protocol, std::any light) {
        lightsMap_[protocol].push_back(light);
    }

    template <std::ranges::range R>
    void addLights(rp::Protocol protocol, R&& lights) {
        if constexpr (std::ranges::sized_range<R>) {
            auto& cont = lightsMap_[protocol];
            cont.reserve(std::ranges::size(lights) + cont.size());
        }

        if constexpr (std::is_lvalue_reference_v< std::remove_cv_t<R> >) {
            lightsMap_[protocol].insert( std::end(lightsMap_[protocol]),
                std::begin(lights), std::end(lights)
            );
        } else {
            lightsMap_[protocol].insert( std::end(lightsMap_[protocol]),
                std::move_iterator(std::begin(lights)),
                std::move_iterator(std::end(lights))
            );
        }
    }

    void setLights(rp::Protocol protocol, Cont<std::any>&& lights) {
        lightsMap_[protocol] = std::move(lights);
    }

private:
    std::map<rp::Protocol, Cont<Fragment>> fragmentsMap_;
    std::map<rp::Protocol, Cont<std::any>> lightsMap_;
};

}   // namespace d3d12

}   // namespace gfx

#endif // __D3D12_SCENE_HPP