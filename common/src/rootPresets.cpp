#include "rootPresets.hpp"

#include <array>
#include <limits>

namespace gfx {

namespace d3d12 {

namespace {

wrl::ComPtr<ID3DBlob> serializeRoot(const D3D12_ROOT_SIGNATURE_DESC& desc) {
    auto blob = wrl::ComPtr<ID3DBlob>();
    auto err = wrl::ComPtr<ID3DBlob>();

    DX_THROW_FAILED( D3D12SerializeRootSignature(
        &desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err
    ) );
    if (err) {
        throw GFX_EXCEPT( static_cast<const char*>(err->GetBufferPointer()) );
    }

    return blob;
}

wrl::ComPtr<ID3D12RootSignature> rootPresetNull(Core& core) {
    auto desc = D3D12_ROOT_SIGNATURE_DESC{
        .NumParameters = 0,
        .pParameters = nullptr,
        .NumStaticSamplers = 0,
        .pStaticSamplers = nullptr,
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    };

    auto blob = serializeRoot(desc);

    auto pDevice = static_cast<ID3D12Device*>( DeviceFetcher::device(core) );
    auto ret = wrl::ComPtr<ID3D12RootSignature>();

    DX_THROW_FAILED( pDevice->CreateRootSignature(
        0, blob->GetBufferPointer(), blob->GetBufferSize(),
        __uuidof(ID3D12RootSignature), &ret
    ) );

    return ret;
}

wrl::ComPtr<ID3D12RootSignature> rootPresetSolid(Core& core) {
    auto params = std::array<D3D12_ROOT_PARAMETER, 1>{
        D3D12_ROOT_PARAMETER{
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
            .Constants = D3D12_ROOT_CONSTANTS{
                .ShaderRegister = 0u,
                .RegisterSpace = 0u,
                .Num32BitValues = 20u
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
        },
    };

    auto desc = D3D12_ROOT_SIGNATURE_DESC{
        .NumParameters = static_cast<UINT>( params.size() ),
        .pParameters = params.data(),
        .NumStaticSamplers = 0,
        .pStaticSamplers = nullptr,
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    };

    auto blob = serializeRoot(desc);

    auto pDevice = static_cast<ID3D12Device*>( DeviceFetcher::device(core) );
    auto ret = wrl::ComPtr<ID3D12RootSignature>();

    DX_THROW_FAILED( pDevice->CreateRootSignature(
        0, blob->GetBufferPointer(), blob->GetBufferSize(),
        __uuidof(ID3D12RootSignature), &ret
    ) );

    return ret;
}

wrl::ComPtr<ID3D12RootSignature> rootPresetUnified(Core& core) {
    auto params = std::array<D3D12_ROOT_PARAMETER, 5>{
        D3D12_ROOT_PARAMETER{   // Per Instance Data
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
            .Descriptor = D3D12_ROOT_DESCRIPTOR{
                .ShaderRegister = 0u,
                .RegisterSpace = 0u
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
        },
        D3D12_ROOT_PARAMETER{   // Materials
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
            .Descriptor = D3D12_ROOT_DESCRIPTOR{
                .ShaderRegister = 1u,
                .RegisterSpace = 0u
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
        },
        D3D12_ROOT_PARAMETER{   // Lights
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
            .Descriptor = D3D12_ROOT_DESCRIPTOR{
                .ShaderRegister = 2u,
                .RegisterSpace = 0u
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
        },
        D3D12_ROOT_PARAMETER{   // Per Draw call Data
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
            .Descriptor = D3D12_ROOT_DESCRIPTOR{
                .ShaderRegister = 0u,
                .RegisterSpace = 0u
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
        },
        D3D12_ROOT_PARAMETER{   // Per Frame Data
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
            .Descriptor = D3D12_ROOT_DESCRIPTOR{
                .ShaderRegister = 1u,
                .RegisterSpace = 0u
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
        },
    };

    auto desc = D3D12_ROOT_SIGNATURE_DESC{
        .NumParameters = static_cast<UINT>( params.size() ),
        .pParameters = params.data(),
        .NumStaticSamplers = 0,
        .pStaticSamplers = nullptr,
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    };

    auto blob = serializeRoot(desc);

    auto pDevice = static_cast<ID3D12Device*>( DeviceFetcher::device(core) );
    auto ret = wrl::ComPtr<ID3D12RootSignature>();

    DX_THROW_FAILED( pDevice->CreateRootSignature(
        0, blob->GetBufferPointer(), blob->GetBufferSize(),
        __uuidof(ID3D12RootSignature), &ret
    ) );

    return ret;
}

wrl::ComPtr<ID3D12RootSignature> rootPresetUnified1(Core& core) {
    auto textureSrvRange = D3D12_DESCRIPTOR_RANGE{
        .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
        .NumDescriptors = static_cast<UINT>(-1),
        .BaseShaderRegister = 1u,
        .RegisterSpace = 1u,
        .OffsetInDescriptorsFromTableStart = 0
    };    

    auto params = std::array<D3D12_ROOT_PARAMETER, 6>{
        D3D12_ROOT_PARAMETER{   // Per Instance Data
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
            .Descriptor = D3D12_ROOT_DESCRIPTOR{
                .ShaderRegister = 0u,
                .RegisterSpace = 0u
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
        },
        D3D12_ROOT_PARAMETER{   // Materials
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
            .Descriptor = D3D12_ROOT_DESCRIPTOR{
                .ShaderRegister = 1u,
                .RegisterSpace = 0u
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
        },
        D3D12_ROOT_PARAMETER{   // Material Textures
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
            .DescriptorTable = D3D12_ROOT_DESCRIPTOR_TABLE {
                .NumDescriptorRanges = 1u,
                .pDescriptorRanges = &textureSrvRange
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
        },
        D3D12_ROOT_PARAMETER{   // Lights
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
            .Descriptor = D3D12_ROOT_DESCRIPTOR{
                .ShaderRegister = 2u,
                .RegisterSpace = 0u
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
        },
        D3D12_ROOT_PARAMETER{   // Per Draw call Data
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
            .Descriptor = D3D12_ROOT_DESCRIPTOR{
                .ShaderRegister = 0u,
                .RegisterSpace = 0u
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
        },
        D3D12_ROOT_PARAMETER{   // Per Frame Data
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
            .Descriptor = D3D12_ROOT_DESCRIPTOR{
                .ShaderRegister = 1u,
                .RegisterSpace = 0u
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
        },
    };

    auto samplerDesc = D3D12_STATIC_SAMPLER_DESC{
        .Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        .AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
        .AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
        .AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
        .MipLODBias = 0.0f,
        .MaxAnisotropy = 0,
        .ComparisonFunc = D3D12_COMPARISON_FUNC_NONE,
        .BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
        .MinLOD = 0.0f,
        .MaxLOD = std::numeric_limits<float>::max(),
        .ShaderRegister = 0u,
        .RegisterSpace = 0u,
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
    };

    auto desc = D3D12_ROOT_SIGNATURE_DESC{
        .NumParameters = static_cast<UINT>(params.size()),
        .pParameters = params.data(),
        .NumStaticSamplers = 1u,
        .pStaticSamplers = &samplerDesc,
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    };

    auto blob = serializeRoot(desc);

    auto pDevice = static_cast<ID3D12Device*>(DeviceFetcher::device(core));
    auto ret = wrl::ComPtr<ID3D12RootSignature>();

    DX_THROW_FAILED(pDevice->CreateRootSignature(
        0, blob->GetBufferPointer(), blob->GetBufferSize(),
        __uuidof(ID3D12RootSignature), &ret
    ));

    return ret;
}

}   // namespace gfx::d3d12::<unnamed>

wrl::ComPtr<ID3D12RootSignature> makeRootPreset(Core& core, RootPreset preset) {
    switch (preset) {
    case RootPreset::Null:
        return rootPresetNull(core);

    case RootPreset::Solid:
        return rootPresetSolid(core);

    case RootPreset::Unified:
        return rootPresetUnified(core);

    case RootPreset::Unified1:
        return rootPresetUnified1(core);

    default:
        throw GFX_EXCEPT("Invalid RootPreset");
    }  
}

Core::RootIdx rootName(RootPreset preset) {
    switch (preset) {
    case RootPreset::Null:
        return "NullRoot";

    case RootPreset::Solid:
        return "SolidRoot";

    case RootPreset::Unified:
        return "UnifiedRoot";

    case RootPreset::Unified1:
        return "UnifiedRoot1";

    default:
        throw GFX_EXCEPT("Invalid RootPreset");
    }
}

} // namespace d3d12

} // namespace gfx