#include "d3d12scene.hpp"

#include "dxMathUtil.hpp"

#include <ranges>
#include <algorithm>
#include <iterator>
#include <execution>

namespace gfx {

namespace d3d12 {

Generator<DrawInfo> CameraScene::iteration() const {
    // As it is expected to be not too many lights,
    // store seperated light or material's pointers in a vector
    // which gives more light or material management flexibility.

    auto lightInfo = DrawInfo();

    auto tmpLightBuffer = std::vector<sr::PhongLight>();
    std::ranges::copy( lights_ | std::views::transform(
        [](auto pLight) { return *pLight; }
    ), std::back_inserter(tmpLightBuffer) );

    lightInfo.set(typeIdx, DIType::Light);
    lightInfo.set(lightIdx, std::span(tmpLightBuffer));

    co_yield std::move(lightInfo);


    auto materialInfo = DrawInfo();

    auto tmpMaterialBuffer = std::vector<sr::PhongMaterial>();
    std::ranges::copy( materials_ | std::views::transform(
        [](auto pMaterial) { return *pMaterial; }
    ), std::back_inserter(tmpMaterialBuffer) );

    materialInfo.set(typeIdx, DIType::Material);
    materialInfo.set(materialIdx, std::span(tmpMaterialBuffer));

    co_yield std::move(materialInfo);


    auto pfdInfo = DrawInfo();
    
    pfdInfo.set( typeIdx, DIType::PFD );
    pfdInfo.set( PFDIdx, sr::BasicPFD {
        .globalAmbientLight = { 0.1f, 0.1f, 0.1f, 1.f } // ,
        // .lightCnt = static_cast<std::uint32_t>( lights_.size() )
    } );

    co_yield std::move(pfdInfo);

    for (const auto& fragment : fragments_) {
        auto fi = fragmentIteration(fragment);
        for (auto& drawInfo : fi) {
            co_yield std::move(drawInfo);
        }
    }
}

Generator<DrawInfo> CameraScene::fragmentIteration(const Fragment& fragment) const {
    auto baseInstIdx = 0u;

    auto meshInfo = DrawInfo();

    meshInfo.set(typeIdx, DIType::Mesh);
    meshInfo.set(meshIdx, fragment.pMesh);

    co_yield std::move(meshInfo);


    auto pidInfo = DrawInfo();

    std::vector<sr::BasicPID> pidBuffer;
    pidBuffer.reserve( fragment.worlds.size() );

    std::ranges::copy( fragment.worlds | std::views::transform(
        [this](const auto& world) {
            return sr::BasicPID {
                .wv = (world * view()).getXmf(),
                .wvp = (world * view() * proj()).getXmf(),
                .normalXform = dx::convertMat<dx::XMFLOAT3X4>(
                    mu::transpose(mu::inverse(world)).get()
                ),
                .matIdx = 0u    // temporary
            };
        }
    ), std::back_inserter(pidBuffer) );

    pidInfo.set(typeIdx, DIType::PID);
    pidInfo.set(PIDIdx, std::span(pidBuffer));

    co_yield std::move(pidInfo);


    auto pddInfo = DrawInfo();

    pddInfo.set(typeIdx, DIType::PDD);
    pddInfo.set(PDDIdx, sr::BasicPDD {
        .instanceIndex = baseInstIdx
    } );
    
    baseInstIdx += fragment.worlds.size();

    co_yield std::move(pddInfo);
}

}   // namespace d3d12

}   // namespace gfx