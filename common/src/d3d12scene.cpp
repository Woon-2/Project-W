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
        requires std::same_as<RenderProtocol, rp::ShadowMapGen>
    Generator<DrawInfo> fragmentIteration(const auto& fragment, const mu::Mat4x4& view, const mu::Mat4x4& proj, auto& baseInstIdx);

    template <rp::PFUnified RenderProtocol>
    Generator<DrawInfo> iterationImpl(const auto& lights, const auto& fragments, const mu::Mat4x4& view, const mu::Mat4x4& proj);
    template <rp::PFUnified RenderProtocol>
    Generator<DrawInfo> fragmentIteration(const auto& fragment, const mu::Mat4x4& view, const mu::Mat4x4& proj, auto& baseInstIdx);
    rp::PhongInstancing::PIDType genPID(rp::PhongInstancing, const mu::Mat4x4& world, const mu::Mat4x4& view, const mu::Mat4x4& proj);
    rp::PhongInstancingNT::PIDType genPID(rp::PhongInstancingNT, const mu::Mat4x4& world, const mu::Mat4x4& view, const mu::Mat4x4& proj);
    rp::ShadowMapGen::PIDType genPID(rp::ShadowMapGen, const mu::Mat4x4& world, const mu::Mat4x4& view, const mu::Mat4x4& proj);

    template <rp::PFUnified RenderProtocol>
    Generator<DrawInfo> iterationImpl(const auto& lights, const auto& fragments, const mu::Mat4x4& view, const mu::Mat4x4& proj) {
        // As it is expected to be not too many lights,
        // store seperated light or material's pointers in a vector
        // which gives more light or material management flexibility.

        using LightType = RenderProtocol::LightType;
        using FLightType = RenderProtocol::FLightType;

        auto lightInfo = DrawInfo();

        auto tmpLightBuffer = FLightType();
        std::ranges::copy( lights | std::views::transform(
            [](const auto& pAnyLight) { return *std::any_cast<const LightType*>(pAnyLight); }
        ), std::back_inserter(tmpLightBuffer) );

        lightInfo.set(RenderProtocol::typeIdx, rp::DIType::Light);
        lightInfo.set(RenderProtocol::lightIdx, std::move(tmpLightBuffer));

        co_yield std::move(lightInfo);

        using PFDType = RenderProtocol::PFDType;
        using FPFDType = RenderProtocol::FPFDType;

        auto pfdInfo = DrawInfo();

        pfdInfo.set(RenderProtocol::typeIdx, rp::DIType::PFD);
        pfdInfo.set(RenderProtocol::PFDIdx, PFDType {
            .globalAmbientLight = { 0.1f, 0.1f, 0.1f, 1.f } // ,
            // .lightCnt = static_cast<std::uint32_t>( lights.size() )
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

        using PIDType = RenderProtocol::PIDType;
        using FPIDType = RenderProtocol::FPIDType;

        auto pidInfo = DrawInfo();

        auto pidBuffer = FPIDType();
        // replace reserve with reserve_if_possible later
        // to support various type of containers
        pidBuffer.reserve( fragment.worlds.size() );

        std::ranges::copy( fragment.worlds | std::views::transform(
            [&view, &proj](const auto& world) {
                return genPID(RenderProtocol{}, world, view, proj);
            }
        ), std::back_inserter(pidBuffer) );

        pidInfo.set(RenderProtocol::typeIdx, rp::DIType::PID);
        pidInfo.set(RenderProtocol::PIDIdx, std::move(pidBuffer));

        co_yield std::move(pidInfo);

        using PDDType = RenderProtocol::PDDType;
        using FPDDType = RenderProtocol::FPDDType;

        auto pddInfo = DrawInfo();

        pddInfo.set(RenderProtocol::typeIdx, rp::DIType::PDD);
        pddInfo.set(RenderProtocol::PDDIdx, FPDDType{
            .material = std::any_cast<typename RenderProtocol::FMaterialType>(
                fragment.pMaterial->as(RenderProtocol::protocol)
            ),
            .instanceIndex = baseInstIdx
        } );
        
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

        auto pidBuffer = FPIDType();
        // replace reserve with reserve_if_possible later
        // to support various type of containers
        pidBuffer.reserve( fragment.worlds.size() );

        std::ranges::copy( fragment.worlds | std::views::transform(
            [&view, &proj](const auto& world) {
                return genPID(RenderProtocol{}, world, view, proj);
            }
        ), std::back_inserter(pidBuffer) );

        pidInfo.set(RenderProtocol::typeIdx, rp::DIType::PID);
        pidInfo.set(RenderProtocol::PIDIdx, std::move(pidBuffer));

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

    rp::PhongInstancing::PIDType genPID(rp::PhongInstancing, const mu::Mat4x4& world, const mu::Mat4x4& view, const mu::Mat4x4& proj) {
        return rp::PhongInstancing::PIDType{
            .wv = mu::transpose(world * view).getXmf(),
            .wvp = mu::transpose(world * view * proj).getXmf(),
            .normalXform = dx::convertMat<dx::XMFLOAT3X3>(
                mu::transpose(mu::inverse(world)).get()
            )
        };
    }

    rp::PhongInstancingNT::PIDType genPID(rp::PhongInstancingNT, const mu::Mat4x4& world, const mu::Mat4x4& view, const mu::Mat4x4& proj) {
        return rp::PhongInstancingNT::PIDType{
            .wv = mu::transpose(world * view).getXmf(),
            .wvp = mu::transpose(world * view * proj).getXmf(),
            .normalXform = dx::convertMat<dx::XMFLOAT3X3>(
                mu::transpose(mu::inverse(world)).get()
            )
        };
    }

    rp::ShadowMapGen::PIDType genPID(rp::ShadowMapGen, const mu::Mat4x4& world, const mu::Mat4x4& view, const mu::Mat4x4& proj) {
        return rp::ShadowMapGen::PIDType{
            .wv = mu::transpose(world * view).getXmf(),
            .wvp = mu::transpose(world * view * proj).getXmf()
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