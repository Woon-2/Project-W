#include "d3d12scene.hpp"

#include "dxMathUtil.hpp"

#include "renderProtocol.hpp"

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

    lightInfo.set(rp::PhongInstancingNT::typeIdx, rp::DIType::Light);
    lightInfo.set(rp::PhongInstancingNT::lightIdx, std::span(tmpLightBuffer));

    co_yield std::move(lightInfo);


    auto materialInfo = DrawInfo();

    auto tmpMaterialBuffer = std::vector<sr::PhongMaterial>();
    std::ranges::copy( materials_ | std::views::transform(
        [](auto pMaterial) { return *pMaterial; }
    ), std::back_inserter(tmpMaterialBuffer) );

    materialInfo.set(rp::PhongInstancingNT::typeIdx,  rp::DIType::Material);
    materialInfo.set(rp::PhongInstancingNT::materialIdx, std::span(tmpMaterialBuffer));

    co_yield std::move(materialInfo);


    auto pfdInfo = DrawInfo();
    
    pfdInfo.set(rp::PhongInstancingNT::typeIdx, rp::DIType::PFD );
    pfdInfo.set(rp::PhongInstancingNT::PFDIdx, sr::BasicPFD {
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

    meshInfo.set(rp::PhongInstancingNT::typeIdx, rp::DIType::Mesh);
    meshInfo.set(rp::PhongInstancingNT::meshIdx, fragment.pMesh);

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

    pidInfo.set(rp::PhongInstancingNT::typeIdx, rp::DIType::PID);
    pidInfo.set(rp::PhongInstancingNT::PIDIdx, std::span(pidBuffer));

    co_yield std::move(pidInfo);


    auto pddInfo = DrawInfo();

    pddInfo.set(rp::PhongInstancingNT::typeIdx, rp::DIType::PDD);
    pddInfo.set(rp::PhongInstancingNT::PDDIdx, sr::BasicPDD {
        .instanceIndex = baseInstIdx
    } );
    
    baseInstIdx += static_cast<std::uint32_t>( fragment.worlds.size() );

    co_yield std::move(pddInfo);
}

}   // namespace d3d12

}   // namespace gfx