#include "shader.hpp"

#include "errorHandling.hpp"

// 셰이더를 컴파일하여 D3D Blob 객체, 그리고 그 객체와 연결된
// D3D12_SHADER_BYTECODE 객체를 리턴한다.
CompiledShaderOutput compileShader(const std::filesystem::path& path,
	const D3D_SHADER_MACRO* macros,
	std::string_view entryPoint, std::string_view target,
	UINT flag1, UINT flag2
) {
	auto ret = CompiledShaderOutput{};
	auto errorBlob = ComPtr<ID3DBlob>{};

	auto hr = D3DCompileFromFile(path.wstring().c_str(), macros, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entryPoint.data(), target.data(), flag1, flag2, &ret.blob, &errorBlob
	);
	DISPLAY_ERROR_DX_HR(hr, false);

	if (errorBlob) {
		if (hr >= 0) {
			DISPLAY_ERROR_STR( false, "[GFX Warning] compileShader: 셰이더 컴파일 중 경고가 발생했습니다.\n"s
				+ static_cast<const char*>(errorBlob->GetBufferPointer()), false
			);
		}
		else {
			DISPLAY_ERROR_STR( false, "[GFX Error] compileShader: 셰이더 컴파일 중 오류가 발생했습니다.\n"s
				+ static_cast<const char*>(errorBlob->GetBufferPointer()), false
			);
			return ret;
		}
	}

	ret.byteCode = D3D12_SHADER_BYTECODE{
		.pShaderBytecode = ret.blob->GetBufferPointer(),
		.BytecodeLength = ret.blob->GetBufferSize()
	};

	return ret;
}

ComPtr<ID3D12PipelineState> createSampleShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	// 셰이더 컴파일
	auto vsCode = compileShader("sample.hlsl", nullptr, "VSMain", "vs_5_1", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);
	auto psCode = compileShader("sample.hlsl", nullptr, "PSMain", "ps_5_1", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

	// 입력 조립기 설정
	auto elemDescs = std::vector<D3D12_INPUT_ELEMENT_DESC>{
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "POSITION",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 0u,
			.AlignedByteOffset = 0u,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "UV",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32_FLOAT,
			.InputSlot = 1u,
			.AlignedByteOffset = 0u,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		}
	};

	auto inputLayoutDesc = D3D12_INPUT_LAYOUT_DESC{
		.pInputElementDescs = elemDescs.data(),
		.NumElements = static_cast<UINT>(elemDescs.size())
	};

	auto psoDesc = D3D12_GRAPHICS_PIPELINE_STATE_DESC{
		.pRootSignature = rootSig,
		.VS = vsCode.byteCode,
		.PS = psCode.byteCode,
		// 블렌드 상태 설정
		.BlendState = D3D12_BLEND_DESC{
			.AlphaToCoverageEnable = false,
			.IndependentBlendEnable = false
		},
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
		// 래스터라이저 설정
		.RasterizerState = D3D12_RASTERIZER_DESC{
			.FillMode = D3D12_FILL_MODE_SOLID,
			.CullMode = D3D12_CULL_MODE_BACK,
			.FrontCounterClockwise = false,
			.DepthBias = 0,
			.DepthBiasClamp = 0.f,
			.SlopeScaledDepthBias = 0.f,
			.DepthClipEnable = true,
			.MultisampleEnable = false,
			.AntialiasedLineEnable = false,
			.ForcedSampleCount = 0u
		},
		// 깊이 스텐실 설정
		.DepthStencilState = D3D12_DEPTH_STENCIL_DESC{
			.DepthEnable = true,
			.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
			.DepthFunc = D3D12_COMPARISON_FUNC_LESS,
			.StencilEnable = false,
			.StencilReadMask = 0u,
			.StencilWriteMask = 0u,
			.FrontFace = D3D12_DEPTH_STENCILOP_DESC{},
			.BackFace = D3D12_DEPTH_STENCILOP_DESC{}
		},
		.InputLayout = inputLayoutDesc,
		// 프리미티브 토폴로지 설정
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.SampleDesc = DXGI_SAMPLE_DESC{
			.Count = 1u, .Quality = 0u
		},
		.NodeMask = 0u,
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	// 렌더 타겟 관련 설정
	psoDesc.NumRenderTargets = 1u;
	psoDesc.BlendState.RenderTarget[0].BlendEnable = false;
	psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);

	setD3DName(ret.Get(), "SampleShader");

	return ret;
}

ComPtr<ID3D12PipelineState> createPBRShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	// 셰이더 컴파일
	auto vsCode = compileShader("pbr.hlsl", nullptr, "VSMain", "vs_5_1", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);
	auto psCode = compileShader("pbr.hlsl", nullptr, "PSMain", "ps_5_1", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

	// 입력 조립기 설정
	auto elemDescs = std::vector<D3D12_INPUT_ELEMENT_DESC>{
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "POSITION",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 0u,
			.AlignedByteOffset = 0u,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "NORMAL",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 1u,
			.AlignedByteOffset = 0u,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "UV",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32_FLOAT,
			.InputSlot = 2u,
			.AlignedByteOffset = 0u,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		}
	};

	auto inputLayoutDesc = D3D12_INPUT_LAYOUT_DESC{
		.pInputElementDescs = elemDescs.data(),
		.NumElements = static_cast<UINT>(elemDescs.size())
	};

	auto psoDesc = D3D12_GRAPHICS_PIPELINE_STATE_DESC{
		.pRootSignature = rootSig,
		.VS = vsCode.byteCode,
		.PS = psCode.byteCode,
		// 블렌드 상태 설정
		.BlendState = D3D12_BLEND_DESC{
			.AlphaToCoverageEnable = false,
			.IndependentBlendEnable = false
		},
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
		// 래스터라이저 설정
		.RasterizerState = D3D12_RASTERIZER_DESC{
			.FillMode = D3D12_FILL_MODE_SOLID,
			.CullMode = D3D12_CULL_MODE_BACK,
			.FrontCounterClockwise = false,
			.DepthBias = 0,
			.DepthBiasClamp = 0.f,
			.SlopeScaledDepthBias = 0.f,
			.DepthClipEnable = true,
			.MultisampleEnable = false,
			.AntialiasedLineEnable = false,
			.ForcedSampleCount = 0u
		},
		// 깊이 스텐실 설정
		.DepthStencilState = D3D12_DEPTH_STENCIL_DESC{
			.DepthEnable = true,
			.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
			.DepthFunc = D3D12_COMPARISON_FUNC_LESS,
			.StencilEnable = false,
			.StencilReadMask = 0u,
			.StencilWriteMask = 0u,
			.FrontFace = D3D12_DEPTH_STENCILOP_DESC{},
			.BackFace = D3D12_DEPTH_STENCILOP_DESC{}
		},
		.InputLayout = inputLayoutDesc,
		// 프리미티브 토폴로지 설정
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.SampleDesc = DXGI_SAMPLE_DESC{
			.Count = 1u, .Quality = 0u
		},
		.NodeMask = 0u,
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	// 렌더 타겟 관련 설정
	psoDesc.NumRenderTargets = 1u;
	psoDesc.BlendState.RenderTarget[0].BlendEnable = false;
	psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);

	setD3DName(ret.Get(), "PBRShader");

	return ret;
}

void RootSig::addParam(const std::string& paramName,
	UINT paramIdx, const D3D12_ROOT_PARAMETER& paramDesc
) {
	auto [_, added] = paramMap_.try_emplace(paramName, paramIdx, paramDesc);
	DISPLAY_ERROR_STR(added, "[GFX Error] RootSig::addParam: 매개변수 "s + paramName
		+ "은(는) 이미 루트 시그너처에 "s + std::to_string(paramMap_.at(paramName).first)
		+ "번 인덱스로 등록되어 있습니다.\n"s, false
	);
}

UINT RootSig::paramIdx(std::string_view paramName) const {
	return paramMap_.at(paramName.data()).first;
}

const D3D12_ROOT_PARAMETER& RootSig::paramDesc(std::string_view paramName) const {
	return paramMap_.at(paramName.data()).second;
}

void DefaultRootSig::build(ID3D12Device* device) {
	auto idxRootParam = 0u;

	// b0: PerDrawcallData
	addParam( "PerDrawcallData", idxRootParam++, D3D12_ROOT_PARAMETER{
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
		.Descriptor = D3D12_ROOT_DESCRIPTOR{
			.ShaderRegister = 0u,
			.RegisterSpace = 0u
		},
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	} );

	// b1: PerFrameData
	addParam( "PerFrameData", idxRootParam++, D3D12_ROOT_PARAMETER{
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
		.Descriptor = D3D12_ROOT_DESCRIPTOR{
			.ShaderRegister = 1u,
			.RegisterSpace = 0u
		},
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	} );

	// t0: PerInstanceData
	addParam( "PerInstanceData", idxRootParam++, D3D12_ROOT_PARAMETER{
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
		.Descriptor = D3D12_ROOT_DESCRIPTOR{
			.ShaderRegister = 0u,
			.RegisterSpace = 0u
		},
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	} );

	// t1: LightData
	addParam( "LightData", idxRootParam++, D3D12_ROOT_PARAMETER{
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
		.Descriptor = D3D12_ROOT_DESCRIPTOR{
			.ShaderRegister = 1u,
			.RegisterSpace = 0u
		},
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	} );

	const auto tex2dRange = D3D12_DESCRIPTOR_RANGE {
		.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		.NumDescriptors = UINT(-1),
		.BaseShaderRegister = 10u,
		.RegisterSpace = 1u,
		.OffsetInDescriptorsFromTableStart = 0u
	};

	// ==== bindless 환경을 고려한 루트 파라미터들 =========

	// t10, space1: TexturePool
	addParam( "TexturePool", idxRootParam++, D3D12_ROOT_PARAMETER{
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
		.DescriptorTable = D3D12_ROOT_DESCRIPTOR_TABLE{
			.NumDescriptorRanges = 1u,
			.pDescriptorRanges = &tex2dRange
		},
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	} );

	const auto texArrayRange = D3D12_DESCRIPTOR_RANGE {
		.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		.NumDescriptors = UINT(-1),
		.BaseShaderRegister = 10u,
		.RegisterSpace = 2u,
		.OffsetInDescriptorsFromTableStart = 0u
	};

	// t10, space2: TextureArrayPool
	addParam( "TextureArrayPool", idxRootParam++, D3D12_ROOT_PARAMETER{
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
		.DescriptorTable = D3D12_ROOT_DESCRIPTOR_TABLE{
			.NumDescriptorRanges = 1u,
			.pDescriptorRanges = &texArrayRange
		},
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	} );

	const auto texCubeRange = D3D12_DESCRIPTOR_RANGE {
		.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		.NumDescriptors = UINT(-1),
		.BaseShaderRegister = 10u,
		.RegisterSpace = 3u,
		.OffsetInDescriptorsFromTableStart = 0u
	};

	// t10, space3: TextureCubePool
	addParam( "TextureCubePool", idxRootParam++, D3D12_ROOT_PARAMETER{
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
		.DescriptorTable = D3D12_ROOT_DESCRIPTOR_TABLE{
			.NumDescriptorRanges = 1u,
			.pDescriptorRanges = &texCubeRange
		},
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	} );

	const auto samRange = D3D12_DESCRIPTOR_RANGE {
		.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
		.NumDescriptors = UINT(-1),
		.BaseShaderRegister = 0u,
		.RegisterSpace = 1u,
		.OffsetInDescriptorsFromTableStart = 0u
	};

	// s0, space1: SamplerPool
	addParam( "SamplerPool", idxRootParam++, D3D12_ROOT_PARAMETER{
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
		.DescriptorTable = D3D12_ROOT_DESCRIPTOR_TABLE{
			.NumDescriptorRanges = 1u,
			.pDescriptorRanges = &samRange
		},
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	} );

	const auto samCmpRange = D3D12_DESCRIPTOR_RANGE {
		.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
		.NumDescriptors = UINT(-1),
		.BaseShaderRegister = 0u,
		.RegisterSpace = 2u,
		.OffsetInDescriptorsFromTableStart = 0u
	};

	// s0, space2: ComparisonSamplerPool
	addParam( "ComparisonSamplerPool", idxRootParam++, D3D12_ROOT_PARAMETER{
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
		.DescriptorTable = D3D12_ROOT_DESCRIPTOR_TABLE{
			.NumDescriptorRanges = 1u,
			.pDescriptorRanges = &samCmpRange
		},
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	} );

	// ==============================================

	// 루트 파라미터들을 d3d12 api에 배열로 넘겨주기 위해
	// 벡터를 만들고, 저장된 파라미터 개수 만큼의 공간을 확보한다.
	// 맵에 저장된 파라미터의 인덱스를 보고 임의 접근하여
	// 파라미터 정보를 채워넣는다.
	auto params = std::vector<D3D12_ROOT_PARAMETER>(paramMap_.size());

	for (const auto& [paramName, paramPair] : paramMap_) {
		auto& paramDesc = paramPair.second;
		auto paramIdx = paramPair.first;

		params[paramIdx] = paramDesc;
	}

	// 루트 시그너처를 직렬화하고 객체로 생성한다.
	auto rootSigDesc = D3D12_ROOT_SIGNATURE_DESC{
		.NumParameters = static_cast<UINT>(params.size()),
		.pParameters = params.data(),
		.NumStaticSamplers = 0u,
		.pStaticSamplers = nullptr,
		.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	};

	auto rootSigBlob = ComPtr<ID3DBlob>{};
	auto errorBlob = ComPtr<ID3DBlob>{};

	DISPLAY_ERROR_DX_VOID(
		D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootSigBlob, &errorBlob),
		false
	);

	if (errorBlob) {
		DISPLAY_ERROR_STR(false, "[GFX Error] DefaultRootSig::build: 루트 시그너처 직렬화 중 오류가 발생했습니다.\n"s
			+ static_cast<const char*>(errorBlob->GetBufferPointer()), false
		);
		return;
	}

	DISPLAY_ERROR_DX_HR( device->CreateRootSignature(
		0u, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(),
		__uuidof(ID3D12RootSignature), &rootSig_
	), false );

	// 이름 설정
	name_ = "DefaultRootSignature";
	setD3DName(rootSig_.Get(), name_);
}

const std::string& DefaultRootSig::name() const {
	return name_;
}