#ifndef __D3D12_SCENE_HPP
#define __D3D12_SCENE_HPP

#include "camera.hpp"
#include "d3d12core.hpp"

#include "d3d12model.hpp"

#include "mathUtil.hpp"
#include "shaderRes.hpp"
#include "drawInfo.hpp"

#include "generator.hpp"

#include <vector>
#include <ranges>
#include <span>

#include "RenderProtocol.hpp"

namespace gfx {

namespace d3d12 {

/**
 * @brief A class representing a scene viewd by a camera in D3D12.    
 * @details As implementing the IScene interface, it can be used in the render loop.     
 * It constructs DrawInfo s with Mesh, world matrix, view matrix, and projection matrix,    
 * (temporarily with color) and returns them in iteration.    
 * 
 * It can be adjusted view frustum culling and LOD later.
 */
class CameraScene : public gfx::CameraScene {
public:
    CameraScene(const Camera& camera)
        : gfx::CameraScene(camera) {}

    Generator<DrawInfo> iteration() const override;

    void addFragment(Fragment&& fragment) {
        fragments_.push_back(std::move(fragment));
    }

    template <std::ranges::range R>
    void addFragments(R&& fragments) {
        fragments_.insert(std::end(fragments_), std::begin(fragments), std::end(fragments));
    }

    void setFragments(std::vector<Fragment>&& fragments) {
        fragments_ = std::move(fragments);
    }

    void addLight(sr::PhongLight* pLight) {
        lights_.push_back(pLight);
    }

    void addMaterial(sr::PhongMaterial* pMaterial) {
        materials_.push_back(pMaterial);
    }

    rp::Protocol protocol() const override {
        return protocol_;
    }

private:
    Generator<DrawInfo> fragmentIteration(const Fragment& fragment) const;

    std::vector<Fragment> fragments_;
    std::vector<sr::PhongLight*> lights_;
    std::vector<sr::PhongMaterial*> materials_;

    rp::Protocol protocol_;
};

}   // namespace d3d12

}   // namespace gfx

#endif // __D3D12_SCENE_HPP

// 