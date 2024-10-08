#include "shadowShader.hpp"

#include "d3d12mesh.hpp"

namespace gfx {

namespace d3d12 {

ShadowShader::ShadowShader(Core &core, const Config &config, std::size_t duplicationCnt) 
    : Shader(),
    internalResArr_(3, std::ranges::range_value_t<decltype(internalResArr_)>(duplicationCnt)),
    resPerFrameData_(core, sizeof(d3d12::sr::BasicPFD), internalResArr_[0], duplicationCnt),
    resPerDrawcallData_(core, sizeof(d3d12::sr::PDDPhong), internalResArr_[1], duplicationCnt),
    resPerInstanceData_(core, sizeof(d3d12::sr::BasicPID) * config.maxInstCnt, internalResArr_[2], duplicationCnt),
    srvHeapStart_( core.descHeapCbvSrvUav().gpuStart() ),
    maxInstances_(config.maxInstCnt) {
    if ( !core.hasDescRange(rp::ShadowMapGen::DescRangeIDShadowTex) ) {
        throw GFX_EXCEPT("[Description] Shadow map descriptor range not found.");
    }

    // pushInputLayout( gfx::makeInputLayoutPreset(gfx::InputLayoutPreset::TexDiffuse) );
    pushProtocol(rp::Protocol::ShadowMapGen);

    auto builder = SimpleShaderBuilder();
    builder.code(Type::Vertex, loadCSO(compiledShaderPath / "shadowMapShader_vs.cso"));

    auto pRoot = core.root(rootName());
    auto pDevice = static_cast<ID3D12Device*>(DeviceFetcher::device(core));

    builder.setRoot(pRoot).build(pDevice, *this, 0u);
}

void ShadowShader::setRootParams(ID3D12GraphicsCommandList* pCmdList, size_t frameIdx) const {
    // temporary
    pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    DX_THROW_FAILED_VOID( pCmdList->SetGraphicsRootShaderResourceView(
        0u, resPerInstanceData_.gpuAddress(frameIdx)
    ) );
    DX_THROW_FAILED_VOID( pCmdList->SetGraphicsRootDescriptorTable(
        1u, srvHeapStart_
    ) );
    DX_THROW_FAILED_VOID( pCmdList->SetGraphicsRootConstantBufferView(
        3u, resPerDrawcallData_.gpuAddress(frameIdx)
    ) );
    DX_THROW_FAILED_VOID( pCmdList->SetGraphicsRootConstantBufferView(
        4u, resPerFrameData_.gpuAddress(frameIdx)
    ) );
}

void ShadowShader::draw( IRenderContext& ctx, const IScene& scene,
    IRenderTarget& target, rp::Protocol protocol
) {
    if (!supports(protocol)) {
        throw GFX_EXCEPT("The protocol is not supported.");
    }

    if ( !ctx.castableTo(RenderContextType::D3D12) ) {
        throw GFX_EXCEPT("The render context is not supported.");
    }

    if ( !target.castableTo(RenderTargetType::D3D12_DEPTH) ) {
        throw GFX_EXCEPT("The render target type is mismatched.");
    }

    auto pCmdList = std::any_cast<wrl::ComPtr<ID3D12GraphicsCommandList>>(
        ctx.cast(RenderContextType::D3D12)
    );

    auto dsv = std::any_cast<Descriptor>(
        target.cast(RenderTargetType::D3D12_DEPTH)
    );

    DX_THROW_FAILED_VOID( pCmdList->OMSetRenderTargets(0u, nullptr, true, &dsv.cpuHandle()) );

    drawImpl( static_cast<D3D12RenderContext&>(ctx), scene, protocol );
}

void ShadowShader::drawImpl( D3D12RenderContext& ctx, const IScene& scene,
    rp::Protocol protocol
) {
    using RenderProtocol = rp::ShadowMapGen;

    using PID = RenderProtocol::PIDType;
    using PFD = RenderProtocol::PFDType;
    using PDD = RenderProtocol::PDDType;

    auto pCmdList = std::any_cast<wrl::ComPtr<ID3D12GraphicsCommandList>>(
        ctx.cast(RenderContextType::D3D12)
    );

    const Mesh* pMesh = nullptr;
    auto instanceCnt = 0ull;

    for ( auto di : scene.iteration(protocol) ) {
        auto type = di.get<rp::DIType>(RenderProtocol::typeIdx);

        switch (type) {
        case rp::DIType::Mesh: {
            pMesh = di.get<const Mesh*>(RenderProtocol::meshIdx);
            pMesh->bind(ctx);
            break;
        }

        case rp::DIType::PID: {
            auto pids = di.get<const RenderProtocol::FPIDType&>(RenderProtocol::PIDIdx);
            auto cnt = std::min(maxInstances_ - instanceCnt, pids.size());
            resPerInstanceData_.uploadRegion(pids.data(), cnt * sizeof(PID), instanceCnt * sizeof(PID), frameIdx());
            instanceCnt += cnt;
            break;
        }

        case rp::DIType::PFD: {
            auto pfd = di.get<const RenderProtocol::FPFDType&>(RenderProtocol::PFDIdx);
            resPerFrameData_.upload(&pfd, sizeof(PFD), frameIdx());
            break;
        }

        case rp::DIType::PDD: {
            auto pdd = di.get<const RenderProtocol::FPDDType&>(RenderProtocol::PDDIdx);
            resPerDrawcallData_.upload(&pdd, sizeof(PDD), frameIdx());

            pMesh->draw(ctx, instanceCnt);
            break;
        }

        default:
            throw std::runtime_error("Invalid DrawInfo type.");
        }
    }
}

}   // namespace gfx::d3d12

}   // namespace gfx