#include "d3d12Drawers.hpp"
#include "d3d12mesh.hpp"

namespace gfx {

namespace d3d12 {

namespace {

template <rp::PFUnified RenderProtocol, PFUnifiedD3d12Shader ConcreteShader>
void illuminanceDrawImpl(const IScene& scene, ConcreteShader& shader, D3D12RenderContext& ctx) {
    using Light = RenderProtocol::LightType;
    using PID = RenderProtocol::PIDType;
    using PFD = RenderProtocol::PFDType;
    using PDD = RenderProtocol::PDDType;

    auto pCmdList = std::any_cast<wrl::ComPtr<ID3D12GraphicsCommandList>>(
        ctx.cast(RenderContextType::D3D12)
    );

    shader.setRootParams(pCmdList.Get());

    auto lightCnt = 0ull;
    const Mesh* pMesh = nullptr;
    auto instanceCnt = 0ull;
    auto pfd = PFD{};

    for (auto di : scene.iteration(RenderProtocol::protocol)) {
        auto type = di.get<rp::DIType>(RenderProtocol::typeIdx);

        switch (type) {
        case rp::DIType::Light: {
            auto lights = di.get<const RenderProtocol::FLightType&>(RenderProtocol::lightIdx);
            auto cnt = std::min(shader.maxLightCnt() - lightCnt, lights.size());
            shader.lights().uploadRegion( lights.data(), cnt * sizeof(Light), lightCnt * sizeof(Light), shader.frameIdx());
            lightCnt += cnt;
            break;
        }

        case rp::DIType::Mesh: {
            pMesh = di.get<const Mesh*>(RenderProtocol::meshIdx);
            pMesh->bind(ctx);
            break;
        }

        case rp::DIType::PID: {
            auto pids = di.get<const RenderProtocol::FPIDType&>(RenderProtocol::PIDIdx);
            auto cnt = std::min(shader.maxInstCnt() - instanceCnt, pids.size());
            shader.pid().uploadRegion(pids.data(), cnt * sizeof(PID), instanceCnt * sizeof(PID), shader.frameIdx());
            instanceCnt += cnt;
            break;
        }

        case rp::DIType::PFD: {
            pfd = di.get<const RenderProtocol::FPFDType&>(RenderProtocol::PFDIdx);
            break;
        }

        case rp::DIType::PDD: {
            auto pdd = di.get<const RenderProtocol::FPDDType&>(RenderProtocol::PDDIdx);
            pfd.lightCnt = static_cast<std::uint32_t>( lightCnt );

            shader.pdd().upload(&pdd, sizeof(PDD), shader.frameIdx());
            shader.pfd().upload(&pfd, sizeof(PFD), shader.frameIdx());

            pMesh->draw(ctx, instanceCnt);
            break;
        }

        default:
            throw std::runtime_error("Invalid DrawInfo type.");
        };
    }
}

}   // namespace gfx::d3d12::<anonymous>

void illuminanceDraw(const IScene& scene, rp::Protocol protocol, Shader& shader, D3D12RenderContext& ctx) {
    switch (protocol) {
    case rp::Protocol::PhongInstancing:
        illuminanceDrawImpl<rp::PhongInstancing>(scene, static_cast<PhongShader&>(shader), ctx);
        break;
    case rp::Protocol::PhongInstancingNT:
        illuminanceDrawImpl<rp::PhongInstancingNT>(scene, static_cast<PhongShaderNT&>(shader), ctx);
        break;
    }
}

}   // namespace gfx::d3d12

}   // namespace gfx