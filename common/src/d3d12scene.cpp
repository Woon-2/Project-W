#include "d3d12scene.hpp"

#include "dxMathUtil.hpp"

#include "renderProtocol.hpp"

#include "gfxExcept.hpp"

#include <ranges>
#include <algorithm>
#include <iterator>
#include <execution>
#include <concepts>

namespace gfx {

namespace d3d12 {

namespace {
    template <class RenderProtocol>
        requires std::same_as<RenderProtocol, rp::ShadowMapGen>
    Generator<DrawInfo> iterationImpl(const auto& fragments, const mu::Mat4x4& view2LightProj, const mu::Mat4x4& view, const mu::Mat4x4& proj);
    template <class RenderProtocol>
        requires std::same_as<RenderProtocol, rp::PhongInstancingShadowed>
    Generator<DrawInfo> iterationImpl(const auto& lights, const auto& fragments, const mu::Mat4x4& view2LightProj, const mu::Mat4x4& view, const mu::Mat4x4& proj);
    template <class RenderProtocol>
        requires std::same_as<RenderProtocol, rp::ShadowMapGen>
    Generator<DrawInfo> fragmentIteration(const auto& fragment, const mu::Mat4x4& view, const mu::Mat4x4& proj, auto& baseInstIdx);

    template <rp::PFUnified RenderProtocol>
    Generator<DrawInfo> iterationImpl(const auto& lights, const auto& fragments, const mu::Mat4x4& view, const mu::Mat4x4& proj);
    template <rp::PFUnified RenderProtocol>
    Generator<DrawInfo> fragmentIteration(const auto& fragment, const mu::Mat4x4& view, const mu::Mat4x4& proj, auto& baseInstIdx);
    template <rp::PFUnified RenderProtocol>
    typename RenderProtocol::FLightType genLights(const auto& lights);
    template <rp::PFUnified RenderProtocol>
    typename RenderProtocol::FPFDType genPFD(const auto& lights);
    template <class RenderProtocol>
        requires std::same_as<RenderProtocol, rp::ShadowMapGen>
    typename RenderProtocol::FPIDType genPID(const auto& worlds, const mu::Mat4x4& view, const mu::Mat4x4& proj);
    template <rp::PFUnified RenderProtocol>
    typename RenderProtocol::FPIDType genPID(const auto& worlds, const mu::Mat4x4& view, const mu::Mat4x4& proj);
    template <rp::PFUnified RenderProtocol>
    typename RenderProtocol::FPDDType genPDD(const auto* pMaterial, auto& baseInstIdx);

    template <rp::PFUnified RenderProtocol>
    Generator<DrawInfo> iterationImpl(const auto& lights, const auto& fragments, const mu::Mat4x4& view, const mu::Mat4x4& proj) {
        // As it is expected to be not too many lights,
        // store seperated light or material's pointers in a vector
        // which gives more light or material management flexibility.

        auto lightInfo = DrawInfo();

        lightInfo.set(RenderProtocol::typeIdx, rp::DIType::Light);
        lightInfo.set(RenderProtocol::lightIdx, genLights<RenderProtocol>(lights));

        co_yield std::move(lightInfo);

        using PFDType = RenderProtocol::PFDType;
        using FPFDType = RenderProtocol::FPFDType;

        auto pfdInfo = DrawInfo();

        pfdInfo.set(RenderProtocol::typeIdx, rp::DIType::PFD);
        pfdInfo.set(RenderProtocol::PFDIdx, genPFD<RenderProtocol>(lights));

        co_yield std::move(pfdInfo);

        auto baseInstIdx = 0u;

        for (const auto& fragment : fragments) {
            auto fi = fragmentIteration<RenderProtocol>(fragment, view, proj, baseInstIdx);
            for (auto& drawInfo : fi) {
                co_yield std::move(drawInfo);
            }
        }
    }

    template <class RenderProtocol>
        requires std::same_as<RenderProtocol, rp::PhongInstancingShadowed>
    Generator<DrawInfo> iterationImpl(const auto& lights, const auto& fragments, const mu::Mat4x4& view2LightProj, const mu::Mat4x4& view, const mu::Mat4x4& proj) {
        // As it is expected to be not too many lights,
        // store seperated light or material's pointers in a vector
        // which gives more light or material management flexibility.

        using LightType = RenderProtocol::LightType;
        using FLightType = RenderProtocol::FLightType;

        auto lightInfo = DrawInfo();

        lightInfo.set(RenderProtocol::typeIdx, rp::DIType::Light);
        lightInfo.set(RenderProtocol::lightIdx, genLights<RenderProtocol>(lights));

        co_yield std::move(lightInfo);

        using PFDType = RenderProtocol::PFDType;
        using FPFDType = RenderProtocol::FPFDType;

        auto pfdInfo = DrawInfo();

        pfdInfo.set(RenderProtocol::typeIdx, rp::DIType::PFD);
        pfdInfo.set(RenderProtocol::PFDIdx, PFDType{
            .view2LightProj = mu::transpose( view2LightProj ).getXmf(),
            .globalAmbientLight = { 0.1f, 0.1f, 0.1f, 1.f }, // ,
            // .lightCnt = static_cast<std::uint32_t>( lights.size() )
            .shadowMapIdx = 0u
        } );

        co_yield std::move(pfdInfo);

        auto baseInstIdx = 0u;

        for (const auto& fragment : fragments) {
            auto fi = fragmentIteration<RenderProtocol>(fragment, view, proj, baseInstIdx);
            for (auto& drawInfo : fi) {
                co_yield std::move(drawInfo);
            }
        }
    }

    template <rp::PFUnified RenderProtocol>
    Generator<DrawInfo> fragmentIteration(const auto& fragment, const mu::Mat4x4& view, const mu::Mat4x4& proj, auto& baseInstIdx) {
        auto meshInfo = DrawInfo();

        meshInfo.set(RenderProtocol::typeIdx, rp::DIType::Mesh);
        meshInfo.set(RenderProtocol::meshIdx, fragment.meshView.mesh());

        co_yield std::move(meshInfo);

        auto pidInfo = DrawInfo();

        pidInfo.set(RenderProtocol::typeIdx, rp::DIType::PID);
        pidInfo.set(RenderProtocol::PIDIdx, genPID<RenderProtocol>(fragment.worlds, view, proj));

        co_yield std::move(pidInfo);

        using PDDType = RenderProtocol::PDDType;
        using FPDDType = RenderProtocol::FPDDType;

        auto pddInfo = DrawInfo();

        pddInfo.set(RenderProtocol::typeIdx, rp::DIType::PDD);
        pddInfo.set(RenderProtocol::PDDIdx, genPDD<RenderProtocol>(fragment.pMaterial, baseInstIdx));
        
        baseInstIdx += static_cast<std::uint32_t>( fragment.worlds.size() );

        co_yield std::move(pddInfo);
    }

    template <class RenderProtocol>
        requires std::same_as<RenderProtocol, rp::ShadowMapGen>
    Generator<DrawInfo> iterationImpl(const auto& fragments, const mu::Mat4x4& view2LightProj, const mu::Mat4x4& view, const mu::Mat4x4& proj) {
        using PFDType = RenderProtocol::PFDType;
        using FPFDType = RenderProtocol::FPFDType;

        auto pfdInfo = DrawInfo();

        pfdInfo.set(RenderProtocol::typeIdx, rp::DIType::PFD);
        pfdInfo.set( RenderProtocol::PFDIdx, PFDType{
            .view2LightProj = mu::transpose( view2LightProj ).getXmf()
        } );

        co_yield std::move(pfdInfo);

        auto baseInstIdx = 0u;

        for (const auto& fragment : fragments) {
            auto fi = fragmentIteration<RenderProtocol>(fragment, view, proj, baseInstIdx);
            for (auto& drawInfo : fi) {
                co_yield std::move(drawInfo);
            }
        }
    }

    template <class RenderProtocol>
        requires std::same_as<RenderProtocol, rp::ShadowMapGen>
    Generator<DrawInfo> fragmentIteration(const auto& fragment, const mu::Mat4x4& view, const mu::Mat4x4& proj, auto& baseInstIdx) {
        auto meshInfo = DrawInfo();

        meshInfo.set(RenderProtocol::typeIdx, rp::DIType::Mesh);
        meshInfo.set(RenderProtocol::meshIdx, fragment.meshView.mesh());

        co_yield std::move(meshInfo);

        using PIDType = RenderProtocol::PIDType;
        using FPIDType = RenderProtocol::FPIDType;

        auto pidInfo = DrawInfo();

        pidInfo.set(RenderProtocol::typeIdx, rp::DIType::PID);
        pidInfo.set(RenderProtocol::PIDIdx, genPID<RenderProtocol>(fragment.worlds, view, proj));

        co_yield std::move(pidInfo);

        using PDDType = RenderProtocol::PDDType;
        using FPDDType = RenderProtocol::FPDDType;

        auto pddInfo = DrawInfo();

        pddInfo.set(RenderProtocol::typeIdx, rp::DIType::PDD);
        pddInfo.set(RenderProtocol::PDDIdx, FPDDType{
            .instanceIndex = baseInstIdx
        } );
        
        baseInstIdx += static_cast<std::uint32_t>( fragment.worlds.size() );

        co_yield std::move(pddInfo);
    }

    template <rp::PFUnified RenderProtocol>
    typename RenderProtocol::FLightType genLights(const auto& lights) {
        using LightType = RenderProtocol::LightType;
        using FLightType = RenderProtocol::FLightType;

        auto lightBuffer = FLightType();
        lightBuffer.reserve( lights.size() );

        std::ranges::copy( lights | std::views::transform(
            [](const auto& pAnyLight) { return *std::any_cast<const LightType*>(pAnyLight); }
        ), std::back_inserter(lightBuffer) );

        return lightBuffer;
    }

    template <rp::PFUnified RenderProtocol>
    typename RenderProtocol::FPFDType genPFD(const auto& lights) {
        using FPFDType = RenderProtocol::FPFDType;

        return FPFDType{
            .globalAmbientLight = { 0.1f, 0.1f, 0.1f, 1.f },
            // .lightCnt = static_cast<std::uint32_t>( lights.size() )
        };
    }

    template <class RenderProtocol>
        requires std::same_as<RenderProtocol, rp::ShadowMapGen>
    typename RenderProtocol::FPIDType genPID(const auto& worlds, const mu::Mat4x4& view, const mu::Mat4x4& proj) {
        using PIDType = RenderProtocol::PIDType;
        using FPIDType = RenderProtocol::FPIDType;

        auto pidBuffer = FPIDType();
        // replace reserve with reserve_if_possible later
        // to support various type of containers
        pidBuffer.reserve( worlds.size() );

        std::ranges::copy( worlds | std::views::transform(
            [&view, &proj](const auto& world) {
                return PIDType{
                    .wv = mu::transpose( world * view ).getXmf(),
                    .wvp = mu::transpose( world * view * proj ).getXmf()
                };
            }
        ), std::back_inserter(pidBuffer) );

        return pidBuffer;
    }

    template <rp::PFUnified RenderProtocol>
    typename RenderProtocol::FPIDType genPID(const auto& worlds, const mu::Mat4x4& view, const mu::Mat4x4& proj) {
        using PIDType = RenderProtocol::PIDType;
        using FPIDType = RenderProtocol::FPIDType;

        auto pidBuffer = FPIDType();
        // replace reserve with reserve_if_possible later
        // to support various type of containers
        pidBuffer.reserve( worlds.size() );

        std::ranges::copy( worlds | std::views::transform(
            [&view, &proj](const auto& world) {
                return PIDType{
                    .wv = mu::transpose( world * view ).getXmf(),
                    .wvp = mu::transpose( world * view * proj ).getXmf(),
                    .normalXform = dx::convertMat<dx::XMFLOAT3X3>(
                        mu::inverse(world * view).get()
                    )
                };
            }
        ), std::back_inserter(pidBuffer) );

        return pidBuffer;
    }

    template <rp::PFUnified RenderProtocol>
    typename RenderProtocol::FPDDType genPDD(const auto* pMaterial, auto& baseInstIdx) {
        using PDDType = RenderProtocol::PDDType;
        using FPDDType = RenderProtocol::FPDDType;

        return FPDDType{
            .material = std::any_cast<typename RenderProtocol::FMaterialType>(
                pMaterial->as(RenderProtocol::protocol)
            ),
            .instanceIndex = baseInstIdx
        };
    }

}   // namespace gfx::d3d12::<anonymous>

Generator<DrawInfo> CameraScene::iteration(rp::Protocol protocol) const {
    const Cont<std::any>* pArgLightsMap = nullptr;
    const Cont<Fragment>* pArgFragmentsMap = nullptr;

    auto tmpArgLightsMap = Cont<std::any>();
    auto tmpArgFragmentsMap = Cont<Fragment>();

    if (lightsMap_.contains(protocol)) {
        pArgLightsMap = &lightsMap_.at(protocol);
    } else {
        pArgLightsMap = &tmpArgLightsMap;
    }

    if (fragmentsMap_.contains(protocol)) {
        pArgFragmentsMap = &fragmentsMap_.at(protocol);
    } else {
        pArgFragmentsMap = &tmpArgFragmentsMap;
    }


    switch (protocol) {
    case rp::Protocol::PhongInstancing: {
        auto coro = iterationImpl<rp::PhongInstancing>(
            *pArgLightsMap, *pArgFragmentsMap, view(), proj()
        );
        for (auto& drawInfo : coro) {
            co_yield std::move(drawInfo);
        }
        break;
    }

    case rp::Protocol::PhongInstancingNT: {
        auto coro = iterationImpl<rp::PhongInstancingNT>(
            *pArgLightsMap, *pArgFragmentsMap, view(), proj()
        );
        for (auto& drawInfo : coro) {
            co_yield std::move(drawInfo);
        }
        break;
    }

    case rp::Protocol::ShadowMapGen: {
        auto coro = iterationImpl<rp::ShadowMapGen>(
            *pArgFragmentsMap, makeView2LightProj(), view(), proj()
        );
        for (auto& drawInfo : coro) {
            co_yield std::move(drawInfo);
        }
        break;
    }

    case rp::Protocol::PhongInstancingShadowed: {
        auto coro = iterationImpl<rp::PhongInstancingShadowed>(
            *pArgLightsMap, *pArgFragmentsMap, makeView2LightProj(), view(), proj()
        );
        for (auto& drawInfo : coro) {
            co_yield std::move(drawInfo);
        }
        break;
    }

    default:
        throw GFX_EXCEPT("[Description] Scene iteration with Invalid protocol.");
    }
}

mu::Mat4x4 CameraScene::makeView2LightProj() const {
    auto pLight = std::any_cast<sr::PhongLight*>( lightsMap_.at(rp::Protocol::ShadowMapGen).front() );
    auto pos = mu::Vec3( dx::XMLoadFloat3(&pLight->posV) );
    auto dir = mu::Vec3( dx::XMLoadFloat3(&pLight->dirV) );
    auto up = mu::Vec3( 0.f, 1.f, 0.f );

    auto lightView = mu::lookAt(pos, pos + dir, up);
    auto lightProj = proj();

    return mu::inverse(view()) * lightView * lightProj;
}

}   // namespace d3d12

}   // namespace gfx