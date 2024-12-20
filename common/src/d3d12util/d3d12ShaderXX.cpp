#include "d3d12util/d3d12ShaderXX.hpp"

#include "shaderPath.hpp"

#include "dxutil/dxexcept.hpp"

#include <d3dcompiler.h>

#include <limits>

namespace gfx {

namespace d3d12 {

std::optional<std::size_t> InputLayout::bindableIdx(const RefMesh& mesh) const {
	for (std::size_t i = 0u; i < mesh.vbLayoutCnt(); ++i) {
		if (checkBindable(mesh.vbs(i))) {
			return i;
		}
	}

	return std::nullopt;
}

void arrangeVBs(RefMesh& refMesh, D3D12Device& device, D3D12GfxCmdList& cmdList,
	std::size_t layoutIdx, const InputLayout& inputLayout
) {
	auto slotProps = std::vector<std::vector<Vertex::Properties>>(inputLayout.slotCnt());
	for (std::size_t i = 0; i < inputLayout.slotCnt(); ++i) {
		const auto& slot = inputLayout.slot(i);

		for (auto j = etoi(Vertex::Properties::Position3D); j < etoi(Vertex::Properties::SIZE); ++j) {
			if (slot.attributes.test(j)) {
				slotProps[i].push_back( static_cast<Vertex::Properties>(j) );
			}
		}
	}

	refMesh.arrangeVBs(device, cmdList, layoutIdx, slotProps);
}

void arrangeVBs(RefModel& refModel, D3D12Device& device, D3D12GfxCmdList& cmdList,
	std::size_t layoutIdx, const InputLayout& inputLayout
) {
	for (auto& node : refModel.nodes()) {
		for (auto& mesh : node.meshes()) {
			arrangeVBs(mesh, device, cmdList, layoutIdx, inputLayout);
		}
	}
}

ShaderBlob::ShaderBlob( const std::filesystem::path& path,
	const InputLayout& inputLayout, const D3D_SHADER_MACRO* macros,
	std::string_view entryPoint, std::string_view target,
	UINT flag1, UINT flag2, Type type
) : dx::DXWrapper<ID3DBlob>(), type_(type) {
	auto errorBlob = wrl::ComPtr<ID3DBlob>{};
	D3DCompileFromFile(path.wstring().c_str(), macros, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entryPoint.data(), target.data(), flag1, flag2, &get(), &errorBlob
	);

	if (errorBlob) {
		OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
	}
}

RenderProtocol::RenderProtocol( D3D12Device& device,
    Shader& shader, const ShaderBlob* pBlobs, std::size_t blobCnt,
	const Desc& desc
) : dx::DXWrapper<InterfaceType>(), pShader_(&shader) {
	auto byteCodes = std::array<D3D12_SHADER_BYTECODE, etoi(ShaderBlob::Type::Size)>{};

	for (std::size_t i = 0; i < blobCnt; ++i) {
		const auto type = pBlobs[i].type();
		const auto byteCode = D3D12_SHADER_BYTECODE{
			.pShaderBytecode = pBlobs[i].get()->GetBufferPointer(),
			.BytecodeLength = pBlobs[i].get()->GetBufferSize()
		};
		byteCodes[etoi(type)] = byteCode;
	}

	auto inputElems = pShader_->inputLayout().makeDescs();

	auto ilDesc = D3D12_INPUT_LAYOUT_DESC{
		.pInputElementDescs = inputElems.data(),
		.NumElements = static_cast<UINT>(inputElems.size())
	};

	auto psoDesc = D3D12_GRAPHICS_PIPELINE_STATE_DESC{
		.pRootSignature = shader.rootSiganture().get().Get(),
		.VS = byteCodes[etoi(ShaderBlob::Type::Vertex)],
		.PS = byteCodes[etoi(ShaderBlob::Type::Pixel)],
		.DS = byteCodes[etoi(ShaderBlob::Type::Domain)],
		.HS = byteCodes[etoi(ShaderBlob::Type::Hull)],
		.GS = byteCodes[etoi(ShaderBlob::Type::Geometry)],
		.StreamOutput = desc.streamOutput,
		.BlendState = desc.blend,
		.SampleMask = desc.sampleMask,
		.RasterizerState = desc.rasterizerState,
		.DepthStencilState = desc.depthStencilState,
		.InputLayout = ilDesc,
		.IBStripCutValue = desc.ibStripCutValue,
		.PrimitiveTopologyType = desc.primitiveTopologyType,
		.NumRenderTargets = desc.numRenderTargets,
		.RTVFormats = { desc.rtvFormats[0], desc.rtvFormats[1], desc.rtvFormats[2], desc.rtvFormats[3],
				desc.rtvFormats[4], desc.rtvFormats[5], desc.rtvFormats[6], desc.rtvFormats[7] },
		.DSVFormat = desc.dsvFormat,
		.SampleDesc = desc.sampleDesc,
		.NodeMask = desc.nodeMask,
		.CachedPSO = desc.cachedPSO,
		.Flags = desc.flags
	};

	device.get()->CreateGraphicsPipelineState(&psoDesc, __uuidof(InterfaceType), &get());
}

namespace detail {

UnifiedRootImpl::UnifiedRootImpl(D3D12Device& device)
	: RootSignature() {
	auto tex2dRange = D3D12_DESCRIPTOR_RANGE {
		.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		.NumDescriptors = UINT(-1),
		.BaseShaderRegister = 10u,
		.RegisterSpace = 1u,
		.OffsetInDescriptorsFromTableStart = 0u
	};

	auto texArrayRange = D3D12_DESCRIPTOR_RANGE {
		.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		.NumDescriptors = UINT(-1),
		.BaseShaderRegister = 10u,
		.RegisterSpace = 2u,
		.OffsetInDescriptorsFromTableStart = 0u
	};

	auto texCubeRange = D3D12_DESCRIPTOR_RANGE {
		.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		.NumDescriptors = UINT(-1),
		.BaseShaderRegister = 10u,
		.RegisterSpace = 3u,
		.OffsetInDescriptorsFromTableStart = 0u
	};

	auto params = std::vector<D3D12_ROOT_PARAMETER>();
	params.reserve(3u + cbvRegisterCnt + srvRegisterCnt + uavRegisterCnt);
	params.push_back( D3D12_ROOT_PARAMETER{
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
		.DescriptorTable = D3D12_ROOT_DESCRIPTOR_TABLE {
			.NumDescriptorRanges = 1u,
			.pDescriptorRanges = &tex2dRange
		} }
	);
	params.push_back( D3D12_ROOT_PARAMETER{
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
		.DescriptorTable = D3D12_ROOT_DESCRIPTOR_TABLE {
			.NumDescriptorRanges = 1u,
			.pDescriptorRanges = &texArrayRange
		} }
	);
	params.push_back( D3D12_ROOT_PARAMETER{
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
		.DescriptorTable = D3D12_ROOT_DESCRIPTOR_TABLE {
			.NumDescriptorRanges = 1u,
			.pDescriptorRanges = &texCubeRange
		} }
	);

	for (UINT i = 0u; i < cbvRegisterCnt; ++i) {
		params.push_back( D3D12_ROOT_PARAMETER{
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
			.Descriptor = D3D12_ROOT_DESCRIPTOR {
				.ShaderRegister = i,
				.RegisterSpace = 0u
			} }
		);
	}

	for (UINT i = 0u; i < srvRegisterCnt; ++i) {
		params.push_back( D3D12_ROOT_PARAMETER{
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
			.Descriptor = D3D12_ROOT_DESCRIPTOR {
				.ShaderRegister = i,
				.RegisterSpace = 0u
			} }
		);
	}

	for (UINT i = 0u; i < uavRegisterCnt; ++i) {
		params.push_back( D3D12_ROOT_PARAMETER{
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV,
			.Descriptor = D3D12_ROOT_DESCRIPTOR {
				.ShaderRegister = i,
				.RegisterSpace = 0u
			} }
		);
	}

	auto samplers = std::vector<D3D12_STATIC_SAMPLER_DESC>();
	samplers.reserve(5u);

	// nearest point wrap
	samplers.emplace_back(
		/* .Filter = */ D3D12_FILTER_MIN_MAG_MIP_POINT,
		/* .AddressU = */ D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		/* .AddressV = */ D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		/* .AddressW = */ D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		/* .MipLODBias = */ 0.f,
		/* .MaxAnisotropy = */ 0u,
		/* .ComparisonFunc = */ D3D12_COMPARISON_FUNC_NEVER,
		/* .BorderColor = */ D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
		/* .MinLOD = */ 0.f,
		/* .MaxLOD = */ std::numeric_limits<float>::max(),
		/* .ShaderRegister = */ 0u,
		/* .RegisterSpace = */ 1u,
		/* .ShaderVisibility = */ D3D12_SHADER_VISIBILITY_ALL
	);

	// trilinear wrap
	samplers.emplace_back(
		/* .Filter = */ D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		/* .AddressU = */ D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		/* .AddressV = */ D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		/* .AddressW = */ D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		/* .MipLODBias = */ 0.f,
		/* .MaxAnisotropy = */ 0u,
		/* .ComparisonFunc = */ D3D12_COMPARISON_FUNC_NEVER,
		/* .BorderColor = */ D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
		/* .MinLOD = */ 0.f,
		/* .MaxLOD = */ std::numeric_limits<float>::max(),
		/* .ShaderRegister = */ 1u,
		/* .RegisterSpace = */ 1u,
		/* .ShaderVisibility = */ D3D12_SHADER_VISIBILITY_ALL
	);

	// nearest border
	samplers.emplace_back(
		/* .Filter = */ D3D12_FILTER_MIN_MAG_MIP_POINT,
		/* .AddressU = */ D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		/* .AddressV = */ D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		/* .AddressW = */ D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		/* .MipLODBias = */ 0.f,
		/* .MaxAnisotropy = */ 0u,
		/* .ComparisonFunc = */ D3D12_COMPARISON_FUNC_NEVER,
		/* .BorderColor = */ D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
		/* .MinLOD = */ 0.f,
		/* .MaxLOD = */ std::numeric_limits<float>::max(),
		/* .ShaderRegister = */ 2u,
		/* .RegisterSpace = */ 1u,
		/* .ShaderVisibility = */ D3D12_SHADER_VISIBILITY_ALL
	);

	// trilinear border
	samplers.emplace_back(
		/* .Filter = */ D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		/* .AddressU = */ D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		/* .AddressV = */ D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		/* .AddressW = */ D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		/* .MipLODBias = */ 0.f,
		/* .MaxAnisotropy = */ 0u,
		/* .ComparisonFunc = */ D3D12_COMPARISON_FUNC_NEVER,
		/* .BorderColor = */ D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
		/* .MinLOD = */ 0.f,
		/* .MaxLOD = */ std::numeric_limits<float>::max(),
		/* .ShaderRegister = */ 3u,
		/* .RegisterSpace = */ 1u,
		/* .ShaderVisibility = */ D3D12_SHADER_VISIBILITY_ALL
	);

	// trilinear comparison
	samplers.emplace_back(
		/* .Filter = */ D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
		/* .AddressU = */ D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		/* .AddressV = */ D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		/* .AddressW = */ D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		/* .MipLODBias = */ 0.f,
		/* .MaxAnisotropy = */ 16u,
		/* .ComparisonFunc = */ D3D12_COMPARISON_FUNC_LESS_EQUAL,
		/* .BorderColor = */ D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,
		/* .MinLOD = */ 0.f,
		/* .MaxLOD = */ std::numeric_limits<float>::max(),
		/* .ShaderRegister = */ 4u,
		/* .RegisterSpace = */ 1u,
		/* .ShaderVisibility = */ D3D12_SHADER_VISIBILITY_ALL
	);

	auto desc = D3D12_ROOT_SIGNATURE_DESC{
		.NumParameters = static_cast<UINT>(params.size()),
		.pParameters = params.data(),
		.NumStaticSamplers = static_cast<UINT>(samplers.size()),
		.pStaticSamplers = samplers.data(),
		.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
			| D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT
	};

    auto blob = wrl::ComPtr<ID3DBlob>();
    auto err = wrl::ComPtr<ID3DBlob>();

    DX_THROW_FAILED( D3D12SerializeRootSignature(
        &desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err
    ) );

    if (err) {
        throw GFX_EXCEPT( static_cast<const char*>(err->GetBufferPointer()) );
    }

	device.get()->CreateRootSignature( 0, blob->GetBufferPointer(), blob->GetBufferSize(),
		__uuidof(InterfaceType), &get()
	);
}

}	// namespace gfx::d3d12::detail

detail::UnifiedRootImpl UnifiedRoot::impl_;

sr::PBRMaterial sr::PBRMaterial::convert(const Material& material) {
	return PBRMaterial{
		.albedoConstant = material.constant<dx::XMFLOAT4>(Material::ConstantType::Albedo),
		.roughnessConstant = material.constant<float>(Material::ConstantType::Roughness),
		.metallicConstant = material.constant<float>(Material::ConstantType::Metallic),
		.albedoConstantMapRatio = material.constant<float>(Material::ConstantType::AlbedoConstantMapRatio),
		.roughnessConstantMapRatio = material.constant<float>(Material::ConstantType::RoughnessConstantMapRatio),
		.metallicConstantMapRatio = material.constant<float>(Material::ConstantType::MetallicConstantMapRatio),
		.emmisiveConstant = material.constant<dx::XMFLOAT3>(Material::ConstantType::Emmisive),
		.emmisiveConstantMapRatio = material.constant<float>(Material::ConstantType::EmmisiveConstantMapRatio),
		.ambientOcclusionConstant = material.constant<float>(Material::ConstantType::AmbientOcclusion),
		.ambientOcclusionConstantMapRatio = material.constant<float>(Material::ConstantType::AmbientOcclusionConstantMapRatio),
		.albedoMapRef = material.mapRef(Material::MapType::Albedo).toxm(),
		.roughnessMapRef = material.mapRef(Material::MapType::Roughness).toxm(),
		.normalMapRef = material.mapRef(Material::MapType::Normal).toxm(),
		.metallicMapRef = material.mapRef(Material::MapType::Metallic).toxm(),
		.metallicSmoothnessMapRef = material.mapRef(Material::MapType::MetallicSmoothness).toxm(),
		.emmisiveMapRef = material.mapRef(Material::MapType::Emmisive).toxm(),
		.ambientOcclusionMapRef = material.mapRef(Material::MapType::AmbientOcclusion).toxm()
	};
}

ShaderPBRIllumination::ShaderPBRIllumination( D3D12Device& device, const RootSignature& root,
	const Config& config, InputLayout::Spec ilSpec
) : Shader(root, makeInputLayout(ilSpec)),
	cbDrawcallDataSize_( calcConstantBufferSize(sizeof(sr::PerDrawcallData0)) ),
	perConfigurationData_(device, sizeof(sr::PerConfigurationData0)),
	perFrameData_(device, sizeof(sr::PerFrameData0)),
	perDrawcallData_(device, cbDrawcallDataSize_ * config.maxDrawcallCnt),
	perInstanceData_(device, sizeof(sr::PerInstanceData0) * config.maxInstanceCnt),
	lightBuffer_(device, sizeof(sr::Light) * config.maxLightCnt),
	maxInstanceCnt_(config.maxInstanceCnt), maxLightCnt_(config.maxLightCnt),
	maxDrawcallCnt_(config.maxDrawcallCnt) {
	perConfigurationData_.pullGpuAddr();
	perFrameData_.pullGpuAddr();
	perDrawcallData_.pullGpuAddr();
	perInstanceData_.pullGpuAddr();
	lightBuffer_.pullGpuAddr();
}

void ShaderPBRIllumination::bindRootParams(D3D12GfxCmdList& cmdList) {
	auto& root = UnifiedRoot::get();

	cmdList.get()->SetGraphicsRootConstantBufferView(
		root.params[ UnifiedRoot::ParamIndices::b0 ],
		perConfigurationData_.gpuAddr()
	);
	cmdList.get()->SetGraphicsRootConstantBufferView(
		root.params[ UnifiedRoot::ParamIndices::b1 ],
		perDrawcallData_.gpuAddr()
	);
	cmdList.get()->SetGraphicsRootConstantBufferView(
		root.params[ UnifiedRoot::ParamIndices::b2 ],
		perFrameData_.gpuAddr()
	);
	cmdList.get()->SetGraphicsRootShaderResourceView(
		root.params[ UnifiedRoot::ParamIndices::t0 ],
		perInstanceData_.gpuAddr()
	);
	cmdList.get()->SetGraphicsRootShaderResourceView(
		root.params[ UnifiedRoot::ParamIndices::t1 ],
		lightBuffer_.gpuAddr()
	);
}

void ShaderPBRIllumination::bindPerDrawcallData(
	std::size_t drawcallIdx, D3D12GfxCmdList& cmdList
) {
	cmdList.get()->SetGraphicsRootConstantBufferView(
		UnifiedRoot::get().params[ UnifiedRoot::ParamIndices::b1 ],
		perDrawcallData_.gpuAddr() + cbDrawcallDataSize() * drawcallIdx
	);
}

void ShaderPBRIllumination::loadBlobs() {
	blobs_[etoi(ShaderBlob::Type::Vertex)] = ShaderBlob{
		shaderPath/"pbrShader.hlsl", inputLayout(), nullptr,
		"VSMain", "vs_5_1", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0, ShaderBlob::Type::Vertex
	};
	blobs_[etoi(ShaderBlob::Type::Pixel)] = ShaderBlob{
		shaderPath/"pbrShader.hlsl", inputLayout(), nullptr,
		"PSMain", "ps_5_1", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0, ShaderBlob::Type::Pixel
	};
}

void ShaderPBRIllumination::releaseBlobs() {
	blobs_[etoi(ShaderBlob::Type::Vertex)].reset();
	blobs_[etoi(ShaderBlob::Type::Pixel)].reset();
}

InputLayout ShaderPBRIllumination::makeInputLayout(InputLayout::Spec ilSpec) {
	switch (ilSpec) {
	case InputLayout::Spec::serial:
		return makeInputLayoutSerial();
	case InputLayout::Spec::separated:
		return makeInputLayoutSeparated();
	default:
		throw GFX_EXCEPT( "Invalid input layout specification." );
	}
}

InputLayout ShaderPBRIllumination::makeInputLayoutSerial() {
	return InputLayout( std::vector<InputLayout::Slot>{
		InputLayout::Slot{
			.elems = {
				InputLayout::Elem{ .semanticName = "POSITION", .semanticIndex = 0u, .format = DXGI_FORMAT_R32G32B32_FLOAT },
				InputLayout::Elem{ .semanticName = "NORMAL", .semanticIndex = 0u, .format = DXGI_FORMAT_R32G32B32_FLOAT },
				InputLayout::Elem{ .semanticName = "TANGENT", .semanticIndex = 0u, .format = DXGI_FORMAT_R32G32B32_FLOAT },
				InputLayout::Elem{ .semanticName = "BITANGENT", .semanticIndex = 0u, .format = DXGI_FORMAT_R32G32B32_FLOAT },
				InputLayout::Elem{ .semanticName = "TEXCOORD", .semanticIndex = 0u, .format = DXGI_FORMAT_R32G32_FLOAT }
			},
			.attributes = (1ull << etoi(Vertex::Properties::Position3D))
				| (1ull << etoi(Vertex::Properties::Normal3D))
				| (1ull << etoi(Vertex::Properties::Tangent3D))
				| (1ull << etoi(Vertex::Properties::Bitangent3D))
				| (1ull << etoi(Vertex::Properties::TexCoord2D0))
		}
	} );
}

InputLayout ShaderPBRIllumination::makeInputLayoutSeparated() {
	return InputLayout( std::vector<InputLayout::Slot>{
		InputLayout::Slot{
			.elems = {
				InputLayout::Elem{ .semanticName = "POSITION", .semanticIndex = 0u, .format = DXGI_FORMAT_R32G32B32_FLOAT },
			},
			.attributes = (1ull << etoi(Vertex::Properties::Position3D))
		},
		InputLayout::Slot{
			.elems = {
				InputLayout::Elem{ .semanticName = "NORMAL", .semanticIndex = 0u, .format = DXGI_FORMAT_R32G32B32_FLOAT },
			},
			.attributes = (1ull << etoi(Vertex::Properties::Normal3D))
		},
		InputLayout::Slot{
			.elems = {
				InputLayout::Elem{ .semanticName = "TANGENT", .semanticIndex = 0u, .format = DXGI_FORMAT_R32G32B32_FLOAT },
			},
			.attributes = (1ull << etoi(Vertex::Properties::Tangent3D))
		},
		InputLayout::Slot{
			.elems = {
				InputLayout::Elem{ .semanticName = "BITANGENT", .semanticIndex = 0u, .format = DXGI_FORMAT_R32G32B32_FLOAT },
			},
			.attributes = (1ull << etoi(Vertex::Properties::Bitangent3D))
		},
		InputLayout::Slot{
			.elems = {
				InputLayout::Elem{ .semanticName = "TEXCOORD", .semanticIndex = 0u, .format = DXGI_FORMAT_R32G32_FLOAT },
			},
			.attributes = (1ull << etoi(Vertex::Properties::TexCoord2D0))
		}
	} );
}

ShaderPBRIlluminationTerrain::ShaderPBRIlluminationTerrain( D3D12Device& device, const RootSignature& root,
	const Config& config, InputLayout::Spec ilSpec
) : Shader(root, makeInputLayout(ilSpec)),
	cbDrawcallDataSize_( calcConstantBufferSize(sizeof(sr::PerDrawcallData0)) ),
	perConfigurationData_(device, sizeof(sr::PerConfigurationData0)),
	perFrameData_(device, sizeof(sr::PerFrameData0)),
	perDrawcallData_(device, cbDrawcallDataSize_ * config.maxDrawcallCnt),
	perInstanceData_(device, sizeof(sr::PerInstanceData1) * config.maxInstanceCnt),
	lightBuffer_(device, sizeof(sr::Light) * config.maxLightCnt),
	maxInstanceCnt_(config.maxInstanceCnt), maxLightCnt_(config.maxLightCnt),
	maxDrawcallCnt_(config.maxDrawcallCnt) {
	perConfigurationData_.pullGpuAddr();
	perFrameData_.pullGpuAddr();
	perDrawcallData_.pullGpuAddr();
	perInstanceData_.pullGpuAddr();
	lightBuffer_.pullGpuAddr();
}

void ShaderPBRIlluminationTerrain::bindRootParams(D3D12GfxCmdList& cmdList) {
	auto& root = UnifiedRoot::get();

	cmdList.get()->SetGraphicsRootConstantBufferView(
		root.params[ UnifiedRoot::ParamIndices::b0 ],
		perConfigurationData_.gpuAddr()
	);
	cmdList.get()->SetGraphicsRootConstantBufferView(
		root.params[ UnifiedRoot::ParamIndices::b2 ],
		perFrameData_.gpuAddr()
	);
	cmdList.get()->SetGraphicsRootShaderResourceView(
		root.params[ UnifiedRoot::ParamIndices::t0 ],
		perInstanceData_.gpuAddr()
	);
	cmdList.get()->SetGraphicsRootShaderResourceView(
		root.params[ UnifiedRoot::ParamIndices::t1 ],
		lightBuffer_.gpuAddr()
	);
}

void ShaderPBRIlluminationTerrain::bindPerDrawcallData(
	std::size_t drawcallIdx, D3D12GfxCmdList& cmdList
) {
	cmdList.get()->SetGraphicsRootConstantBufferView(
		UnifiedRoot::get().params[ UnifiedRoot::ParamIndices::b1 ],
		perDrawcallData_.gpuAddr() + cbDrawcallDataSize() * drawcallIdx
	);
}

void ShaderPBRIlluminationTerrain::loadBlobs() {
	blobs_[etoi(ShaderBlob::Type::Vertex)] = ShaderBlob{
		shaderPath/"pbrShaderTerrain.hlsl", inputLayout(), nullptr,
		"VSMain", "vs_5_1", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0, ShaderBlob::Type::Vertex
	};
	blobs_[etoi(ShaderBlob::Type::Pixel)] = ShaderBlob{
		shaderPath/"pbrShaderTerrain.hlsl", inputLayout(), nullptr,
		"PSMain", "ps_5_1", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0, ShaderBlob::Type::Pixel
	};
	blobs_[etoi(ShaderBlob::Type::Hull)] = ShaderBlob{
		shaderPath/"pbrShaderTerrain.hlsl", inputLayout(), nullptr,
		"HSMain", "hs_5_1", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0, ShaderBlob::Type::Hull
	};
	blobs_[etoi(ShaderBlob::Type::Domain)] = ShaderBlob{
		shaderPath/"pbrShaderTerrain.hlsl", inputLayout(), nullptr,
		"DSMain", "ds_5_1", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0, ShaderBlob::Type::Domain
	};
}

void ShaderPBRIlluminationTerrain::releaseBlobs() {
	blobs_[etoi(ShaderBlob::Type::Vertex)].reset();
	blobs_[etoi(ShaderBlob::Type::Pixel)].reset();
	blobs_[etoi(ShaderBlob::Type::Hull)].reset();
	blobs_[etoi(ShaderBlob::Type::Domain)].reset();
}

InputLayout ShaderPBRIlluminationTerrain::makeInputLayout(InputLayout::Spec ilSpec) {
	switch (ilSpec) {
	case InputLayout::Spec::serial:
		return makeInputLayoutSerial();
	case InputLayout::Spec::separated:
		return makeInputLayoutSeparated();
	default:
		throw GFX_EXCEPT( "Invalid input layout specification." );
	}
}

InputLayout ShaderPBRIlluminationTerrain::makeInputLayoutSerial() {
	return InputLayout( std::vector<InputLayout::Slot>{
		InputLayout::Slot{
			.elems = {
				InputLayout::Elem{ .semanticName = "POSITION", .semanticIndex = 0u, .format = DXGI_FORMAT_R32G32B32_FLOAT },
				InputLayout::Elem{ .semanticName = "NORMAL", .semanticIndex = 0u, .format = DXGI_FORMAT_R32G32B32_FLOAT },
				InputLayout::Elem{ .semanticName = "TEXCOORD", .semanticIndex = 0u, .format = DXGI_FORMAT_R32G32_FLOAT }
			},
			.attributes = (1ull << etoi(Vertex::Properties::Position3D))
				| (1ull << etoi(Vertex::Properties::Normal3D))
				| (1ull << etoi(Vertex::Properties::TexCoord2D0))
		}
	} );
}

InputLayout ShaderPBRIlluminationTerrain::makeInputLayoutSeparated() {
	return InputLayout( std::vector<InputLayout::Slot>{
		InputLayout::Slot{
			.elems = {
				InputLayout::Elem{ .semanticName = "POSITION", .semanticIndex = 0u, .format = DXGI_FORMAT_R32G32B32_FLOAT },
			},
			.attributes = (1ull << etoi(Vertex::Properties::Position3D))
		},
		InputLayout::Slot{
			.elems = {
				InputLayout::Elem{ .semanticName = "NORMAL", .semanticIndex = 0u, .format = DXGI_FORMAT_R32G32B32_FLOAT },
			},
			.attributes = (1ull << etoi(Vertex::Properties::Normal3D))
		},
		InputLayout::Slot{
			.elems = {
				InputLayout::Elem{ .semanticName = "TEXCOORD", .semanticIndex = 0u, .format = DXGI_FORMAT_R32G32_FLOAT },
			},
			.attributes = (1ull << etoi(Vertex::Properties::TexCoord2D0))
		}
	} );
}

}   // namespace gfx::d3d12

}   // namespace gfx