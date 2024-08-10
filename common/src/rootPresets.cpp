#include "rootPresets.hpp"

namespace gfx {

namespace d3d12 {

namespace {
    wrl::ComPtr<ID3D12RootSignature> rootPresetNull(Core& core) {
        auto pDevice = static_cast<ID3D12Device*>( DeviceFetcher::device(core) );

        auto ret = wrl::ComPtr<ID3D12RootSignature>();

        auto desc = D3D12_ROOT_SIGNATURE_DESC{
            .NumParameters = 0,
            .pParameters = nullptr,
            .NumStaticSamplers = 0,
            .pStaticSamplers = nullptr,
            .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
        };

        auto blob = wrl::ComPtr<ID3DBlob>();
        auto err = wrl::ComPtr<ID3DBlob>();

        DX_THROW_FAILED( D3D12SerializeRootSignature(
            &desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err
        ) );
        if (err) {
            throw GFX_EXCEPT( static_cast<const char*>(err->GetBufferPointer()) );
        }
        DX_THROW_FAILED( pDevice->CreateRootSignature(
            0, blob->GetBufferPointer(), blob->GetBufferSize(),
            __uuidof(ID3D12RootSignature), &ret
        ) );

        return ret;
    }
}

wrl::ComPtr<ID3D12RootSignature> makeRootPreset(Core& core, RootPreset preset) {
    switch (preset) {
    case RootPreset::Null:
        return rootPresetNull(core);

    default:
        throw GFX_EXCEPT("Invalid RootPreset");
    }  
}

Core::RootIdx rootName(RootPreset preset) {
    switch (preset) {
    case RootPreset::Null:
        return "NullRoot";

    default:
        throw GFX_EXCEPT("Invalid RootPreset");
    }
}

} // namespace d3d12

} // namespace gfx