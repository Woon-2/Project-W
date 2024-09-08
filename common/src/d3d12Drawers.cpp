#include "d3d12Drawers.hpp"
#include "d3d12mesh.hpp"

namespace gfx {

namespace d3d12 {

namespace {

template <class RenderProtocol, class ConcreteShader>
void illuminanceDrawImpl(const IScene& scene, ConcreteShader& shader, ID3D12GraphicsCommandList* pCmdList) {
    using Light = RenderProtocol::LightType;
    using Material = RenderProtocol::MaterialType;
    using PID = RenderProtocol::PIDType;
    using PFD = RenderProtocol::PFDType;
    using PDD = RenderProtocol::PDDType;

    shader.setRootParams(pCmdList);

    auto lightCnt = 0ull;
    auto materialCnt = 0ull;
    const Mesh* pMesh = nullptr;
    auto instanceCnt = 0ull;
    auto pfd = PFD{};

    for (auto di : scene.iteration()) {
        auto type = di.get<rp::DIType>(RenderProtocol::typeIdx);

        switch (type) {
        case rp::DIType::Light: {
            auto lights = di.get<const RenderProtocol::FLightType&>(RenderProtocol::lightIdx);
            auto cnt = std::min(ConcreteShader::defMaxLights - lightCnt, lights.size());
            shader.lights().uploadRegion( lights.data(), cnt * sizeof(Light), lightCnt * sizeof(Light) );
            lightCnt += cnt;
            break;
        }

        case rp::DIType::Material: {
            auto materials = di.get<const RenderProtocol::FMaterialType&>(RenderProtocol::materialIdx);
            auto cnt = std::min(ConcreteShader::defMaxMaterials - materialCnt, materials.size());
            shader.materials().uploadRegion(materials.data(), cnt * sizeof(Material), materialCnt * sizeof(Material));
            materialCnt += cnt;
            break;
        }

        case rp::DIType::Mesh: {
            pMesh = di.get<const Mesh*>(RenderProtocol::meshIdx);
            pMesh->bind(pCmdList);
            break;
        }

        case rp::DIType::PID: {
            auto pids = di.get<const RenderProtocol::FPIDType&>(RenderProtocol::PIDIdx);
            auto cnt = std::min(ConcreteShader::defMaxInstances - instanceCnt, pids.size());
            shader.pid().uploadRegion(pids.data(), cnt * sizeof(PID), instanceCnt * sizeof(PID));
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

            shader.pdd().upload(&pdd, sizeof(PDD));
            shader.pfd().upload(&pfd, sizeof(PFD));

            pMesh->draw(pCmdList, instanceCnt);
            break;
        }

        default:
            throw std::runtime_error("Invalid DrawInfo type.");
        };
    }
}

}   // namespace gfx::d3d12::<anonymous>

void illuminanceDraw( const IScene& scene, rp::PhongInstancing protocol,
    PhongShader& shader, ID3D12GraphicsCommandList* pCmdList
) {
    illuminanceDrawImpl<rp::PhongInstancing>(scene, shader, pCmdList);
}

void illuminanceDraw( const IScene& scene, rp::PhongInstancingNT protocol,
    PhongShaderNT& shader, ID3D12GraphicsCommandList* pCmdList
) {
    illuminanceDrawImpl<rp::PhongInstancingNT>(scene, shader, pCmdList);
}

}   // namespace gfx::d3d12

}   // namespace gfx