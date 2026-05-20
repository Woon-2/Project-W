#include "pch.hpp"
#include "shader.hpp"
#include "particleModules.hpp"

#include "errorHandling.hpp"

namespace {

struct DxcContext {
	ComPtr<IDxcCompiler3> compiler;
	ComPtr<IDxcUtils> utils;

	DxcContext() {
		DISPLAY_ERROR_DX_HR(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler)), false);
		DISPLAY_ERROR_DX_HR(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils)), false);
	}
};

DxcContext& getDxcContext() {
	static DxcContext ctx;
	return ctx;
}

} // namespace

// 셰이더를 컴파일하여 DXC Blob 객체, 그리고 그 객체와 연결된
// D3D12_SHADER_BYTECODE 객체를 리턴한다.
// flag1, flag2는 하위 호환성을 위해 유지하나 DXC에서는 사용하지 않는다.
CompiledShaderOutput compileShader(const std::filesystem::path& path,
	const D3D_SHADER_MACRO* macros,
	std::string_view entryPoint, std::string_view target,
	UINT flag1, UINT flag2
) {
	auto ret = CompiledShaderOutput{};
	auto& ctx = getDxcContext();

	// 소스 파일 로드
	ComPtr<IDxcBlobEncoding> sourceBlob;
	auto hr = ctx.utils->LoadFile(path.wstring().c_str(), nullptr, &sourceBlob);
	if (FAILED(hr)) {
		DISPLAY_ERROR_DX_HR(hr, false);
		return ret;
	}

	DxcBuffer sourceBuffer{
		.Ptr = sourceBlob->GetBufferPointer(),
		.Size = sourceBlob->GetBufferSize(),
		.Encoding = DXC_CP_UTF8
	};

	// 모든 wstring을 먼저 수집한 뒤 포인터 배열을 구성한다.
	// vector 재할당 시 c_str() 포인터가 무효화되는 것을 방지하기 위함.
	std::vector<std::wstring> argStorage;

	argStorage.push_back(path.wstring());   // 진단용 소스 파일명
	argStorage.push_back(L"-E");
	argStorage.push_back(std::wstring(entryPoint.begin(), entryPoint.end()));
	argStorage.push_back(L"-T");
	argStorage.push_back(std::wstring(target.begin(), target.end()));

	// 매크로 정의
	if (macros) {
		for (auto m = macros; m->Name != nullptr; ++m) {
			auto def = std::string(m->Name);
			if (m->Definition && m->Definition[0] != '\0') {
				def += "=";
				def += m->Definition;
			}
			argStorage.push_back(L"-D");
			argStorage.push_back(std::wstring(def.begin(), def.end()));
		}
	}

#ifdef _DEBUG
	argStorage.push_back(L"-Zi");   // 디버그 정보 포함
	argStorage.push_back(L"-Od");   // 최적화 비활성화
#endif

	// 모든 문자열 확정 후 포인터 배열 구성
	std::vector<LPCWSTR> args;
	args.reserve(argStorage.size());
	for (const auto& s : argStorage) {
		args.push_back(s.c_str());
	}

	ComPtr<IDxcIncludeHandler> includeHandler;
	ctx.utils->CreateDefaultIncludeHandler(&includeHandler);

	// 컴파일
	ComPtr<IDxcResult> result;
	hr = ctx.compiler->Compile(
		&sourceBuffer,
		args.data(), static_cast<UINT32>(args.size()),
		includeHandler.Get(),
		IID_PPV_ARGS(&result)
	);
	DISPLAY_ERROR_DX_HR(hr, false);

	// 컴파일 상태 확인
	HRESULT compileStatus = S_OK;
	result->GetStatus(&compileStatus);

	// 에러/경고 출력
	ComPtr<IDxcBlobUtf8> errors;
	result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
	if (errors && errors->GetStringLength() > 0) {
		if (FAILED(compileStatus)) {
			DISPLAY_ERROR_STR(false, "[GFX Error] compileShader: 셰이더 컴파일 중 오류가 발생했습니다.\n"s
				+ errors->GetStringPointer(), false
			);
			return ret;
		}
		else {
			DISPLAY_ERROR_STR(false, "[GFX Warning] compileShader: 셰이더 컴파일 중 경고가 발생했습니다.\n"s
				+ errors->GetStringPointer(), false
			);
		}
	}

	// 바이트코드 획득
	result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&ret.blob), nullptr);

	if (ret.blob) {
		ret.byteCode = D3D12_SHADER_BYTECODE{
			.pShaderBytecode = ret.blob->GetBufferPointer(),
			.BytecodeLength = ret.blob->GetBufferSize()
		};
	}

	return ret;
}

ComPtr<ID3D12PipelineState> createSampleShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	// 셰이더 컴파일
	auto vsCode = compileShader("sample.hlsl", nullptr, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);
	auto psCode = compileShader("sample.hlsl", nullptr, "PSMain", "ps_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

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
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);

	setD3DName(ret.Get(), "SampleShader");

	return ret;
}

ComPtr<ID3D12PipelineState> createShadowMapShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	// 셰이더 컴파일
	auto vsCode = compileShader("shadowMap.hlsl", nullptr, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

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
		}
	};

	auto inputLayoutDesc = D3D12_INPUT_LAYOUT_DESC{
		.pInputElementDescs = elemDescs.data(),
		.NumElements = static_cast<UINT>(elemDescs.size())
	};

	auto psoDesc = D3D12_GRAPHICS_PIPELINE_STATE_DESC{
		.pRootSignature = rootSig,
		.VS = vsCode.byteCode,
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
			.DepthBias = 1000,
			.DepthBiasClamp = 0.01f,
			.SlopeScaledDepthBias = 2.5f,
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

	// Depth-only: 렌더 타겟 없음
	psoDesc.NumRenderTargets = 0u;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);

	setD3DName(ret.Get(), "ShadowMapShader");

	return ret;
}

ComPtr<ID3D12PipelineState> createShadowMapCSMShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	auto vsCode = compileShader("shadowMapCSM.hlsl", nullptr, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

	auto elemDescs = std::vector<D3D12_INPUT_ELEMENT_DESC>{
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "POSITION",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 0u,
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
		.BlendState = D3D12_BLEND_DESC{
			.AlphaToCoverageEnable = false,
			.IndependentBlendEnable = false
		},
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
		.RasterizerState = D3D12_RASTERIZER_DESC{
			.FillMode = D3D12_FILL_MODE_SOLID,
			.CullMode = D3D12_CULL_MODE_BACK,
			.FrontCounterClockwise = false,
			.DepthBias = 1000,
			.DepthBiasClamp = 0.01f,
			.SlopeScaledDepthBias = 2.5f,
			.DepthClipEnable = true,
			.MultisampleEnable = false,
			.AntialiasedLineEnable = false,
			.ForcedSampleCount = 0u
		},
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
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.SampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
		.NodeMask = 0u,
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};
	psoDesc.NumRenderTargets = 0u;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);
	setD3DName(ret.Get(), "ShadowMapCSMShader");
	return ret;
}

ComPtr<ID3D12PipelineState> createShadowMapSkinnedShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	// 셰이더 컴파일
	auto vsCode = compileShader("shadowMapSkinned.hlsl", nullptr, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

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
			.SemanticName = "BONE_INDICES",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32A32_SINT,
			.InputSlot = 1u,
			.AlignedByteOffset = 0u,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "BONE_WEIGHTS",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
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
			.DepthBias = 1000,
			.DepthBiasClamp = 0.01f,
			.SlopeScaledDepthBias = 2.5f,
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

	// Depth-only: 렌더 타겟 없음
	psoDesc.NumRenderTargets = 0u;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);

	setD3DName(ret.Get(), "ShadowMapSkinnedShader");

	return ret;
}

ComPtr<ID3D12PipelineState> createShadowMapSkinnedCSMShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	auto vsCode = compileShader("shadowMapSkinnedCSM.hlsl", nullptr, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

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
			.SemanticName = "BONE_INDICES",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32A32_SINT,
			.InputSlot = 1u,
			.AlignedByteOffset = 0u,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "BONE_WEIGHTS",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
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
		.BlendState = D3D12_BLEND_DESC{
			.AlphaToCoverageEnable = false,
			.IndependentBlendEnable = false
		},
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
		.RasterizerState = D3D12_RASTERIZER_DESC{
			.FillMode = D3D12_FILL_MODE_SOLID,
			.CullMode = D3D12_CULL_MODE_BACK,
			.FrontCounterClockwise = false,
			.DepthBias = 1000,
			.DepthBiasClamp = 0.01f,
			.SlopeScaledDepthBias = 2.5f,
			.DepthClipEnable = true,
			.MultisampleEnable = false,
			.AntialiasedLineEnable = false,
			.ForcedSampleCount = 0u
		},
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
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.SampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
		.NodeMask = 0u,
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};
	psoDesc.NumRenderTargets = 0u;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);
	setD3DName(ret.Get(), "ShadowMapSkinnedCSMShader");
	return ret;
}

ComPtr<ID3D12PipelineState> createPBRShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	// 셰이더 컴파일
	auto vsCode = compileShader("pbr.hlsl", nullptr, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);
	auto psCode = compileShader("pbr.hlsl", nullptr, "PSMain", "ps_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

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
			.SemanticName = "TANGENT",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 2u,
			.AlignedByteOffset = 0u,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "BITANGENT",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 3u,
			.AlignedByteOffset = 0u,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "UV",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32_FLOAT,
			.InputSlot = 4u,
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
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);

	setD3DName(ret.Get(), "PBRShader");

	return ret;
}

ComPtr<ID3D12PipelineState> createPBRShaderCSMDebug(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	D3D_SHADER_MACRO csmDebugMacros[] = { {"CSM_DEBUG_VIS", "1"}, {nullptr, nullptr} };

	auto vsCode = compileShader("pbr.hlsl", csmDebugMacros, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);
	auto psCode = compileShader("pbr.hlsl", csmDebugMacros, "PSMain", "ps_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

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
			.SemanticName = "TANGENT",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 2u,
			.AlignedByteOffset = 0u,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "BITANGENT",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 3u,
			.AlignedByteOffset = 0u,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "UV",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32_FLOAT,
			.InputSlot = 4u,
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
		.BlendState = D3D12_BLEND_DESC{
			.AlphaToCoverageEnable = false,
			.IndependentBlendEnable = false
		},
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
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
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.SampleDesc = DXGI_SAMPLE_DESC{
			.Count = 1u, .Quality = 0u
		},
		.NodeMask = 0u,
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

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
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);

	setD3DName(ret.Get(), "PBRShaderCSMDebug");

	return ret;
}

ComPtr<ID3D12PipelineState> createPBRSkinnedShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	// 셰이더 컴파일
	auto vsCode = compileShader("pbrSkinned.hlsl", nullptr, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);
	auto psCode = compileShader("pbrSkinned.hlsl", nullptr, "PSMain", "ps_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

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
			.SemanticName = "TANGENT",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 2u,
			.AlignedByteOffset = 0u,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "BITANGENT",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 3u,
			.AlignedByteOffset = 0u,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "UV",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32_FLOAT,
			.InputSlot = 4u,
			.AlignedByteOffset = 0u,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "BONE_INDICES",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32A32_SINT,
			.InputSlot = 5u,
			.AlignedByteOffset = 0u,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "BONE_WEIGHTS",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
			.InputSlot = 6u,
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
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);

	setD3DName(ret.Get(), "PBRSkinnedShader");

	return ret;
}

ComPtr<ID3D12PipelineState> createPBRSkinnedShaderCSMDebug(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	D3D_SHADER_MACRO csmDebugMacros[] = { {"CSM_DEBUG_VIS", "1"}, {nullptr, nullptr} };

	auto vsCode = compileShader("pbrSkinned.hlsl", csmDebugMacros, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);
	auto psCode = compileShader("pbrSkinned.hlsl", csmDebugMacros, "PSMain", "ps_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

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
			.SemanticName = "TANGENT",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 2u,
			.AlignedByteOffset = 0u,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "BITANGENT",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 3u,
			.AlignedByteOffset = 0u,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "UV",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32_FLOAT,
			.InputSlot = 4u,
			.AlignedByteOffset = 0u,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "BONE_INDICES",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32A32_SINT,
			.InputSlot = 5u,
			.AlignedByteOffset = 0u,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "BONE_WEIGHTS",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
			.InputSlot = 6u,
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
		.BlendState = D3D12_BLEND_DESC{
			.AlphaToCoverageEnable = false,
			.IndependentBlendEnable = false
		},
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
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
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.SampleDesc = DXGI_SAMPLE_DESC{
			.Count = 1u, .Quality = 0u
		},
		.NodeMask = 0u,
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

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
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);

	setD3DName(ret.Get(), "PBRSkinnedShaderCSMDebug");

	return ret;
}

ComPtr<ID3D12PipelineState> createSkyboxShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	// 셰이더 컴파일
	auto vsCode = compileShader("skybox.hlsl", nullptr, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);
	auto psCode = compileShader("skybox.hlsl", nullptr, "PSMain", "ps_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

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
			.FrontCounterClockwise = true,
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
			.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
			.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,
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
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);

	setD3DName(ret.Get(), "SkyboxShader");

	return ret;
}

ComPtr<ID3D12PipelineState> createBVShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	// 셰이더 컴파일
	auto vsCode = compileShader("boundingVolume.hlsl", nullptr, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);
	auto psCode = compileShader("boundingVolume.hlsl", nullptr, "PSMain", "ps_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

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
			.FillMode = D3D12_FILL_MODE_WIREFRAME,
			.CullMode = D3D12_CULL_MODE_NONE,
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
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);

	setD3DName( ret.Get(), "BVShader" );

	return ret;
}

ComPtr<ID3D12PipelineState> createUIShader( ID3D12Device* device, ID3D12RootSignature* rootSig )
{
	ComPtr<ID3D12PipelineState> ret{};

	// 셰이더 컴파일
	auto vsCode = compileShader( "ui.hlsl", nullptr, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u );
	auto psCode = compileShader( "ui.hlsl", nullptr, "PSMain", "ps_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u );

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
			.SemanticName = "TEXCOORD",
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
			.CullMode = D3D12_CULL_MODE_NONE,
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
			.DepthEnable = false,
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
	psoDesc.BlendState.RenderTarget[0].BlendEnable = true;
	psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState( &psoDesc, __uuidof(ID3D12PipelineState), &ret ),
		false
	);

	setD3DName( ret.Get(), "UIShader" );

	return ret;
}

ComPtr<ID3D12PipelineState> createTerrainShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	auto vsCode = compileShader("terrain.hlsl", nullptr, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);
	auto psCode = compileShader("terrain.hlsl", nullptr, "PSMain", "ps_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

	// 5-slot input layout: Position (slot 0), Normal (slot 1), Tangent (slot 2), Bitangent (slot 3), UV (slot 4)
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
			.SemanticName = "TANGENT",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 2u,
			.AlignedByteOffset = 0u,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "BITANGENT",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 3u,
			.AlignedByteOffset = 0u,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "UV",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32_FLOAT,
			.InputSlot = 4u,
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
		.BlendState = D3D12_BLEND_DESC{
			.AlphaToCoverageEnable = false,
			.IndependentBlendEnable = false
		},
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
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
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.SampleDesc = DXGI_SAMPLE_DESC{.Count = 1u, .Quality = 0u},
		.NodeMask = 0u,
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

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
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);

	setD3DName(ret.Get(), "TerrainShader");

	return ret;
}

ComPtr<ID3D12PipelineState> createTerrainShaderCSMDebug(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	D3D_SHADER_MACRO csmDebugMacros[] = { {"CSM_DEBUG_VIS", "1"}, {nullptr, nullptr} };

	auto vsCode = compileShader("terrain.hlsl", csmDebugMacros, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);
	auto psCode = compileShader("terrain.hlsl", csmDebugMacros, "PSMain", "ps_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

	// 5-slot input layout: Position (slot 0), Normal (slot 1), Tangent (slot 2), Bitangent (slot 3), UV (slot 4)
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
			.SemanticName = "TANGENT",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 2u,
			.AlignedByteOffset = 0u,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "BITANGENT",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 3u,
			.AlignedByteOffset = 0u,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "UV",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32_FLOAT,
			.InputSlot = 4u,
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
		.BlendState = D3D12_BLEND_DESC{
			.AlphaToCoverageEnable = false,
			.IndependentBlendEnable = false
		},
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
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
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.SampleDesc = DXGI_SAMPLE_DESC{.Count = 1u, .Quality = 0u},
		.NodeMask = 0u,
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

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
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);

	setD3DName(ret.Get(), "TerrainShaderCSMDebug");

	return ret;
}

ComPtr<ID3D12PipelineState> createTerrainDeferredGBufferShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	auto vsCode = compileShader("terrainDeferred.hlsl", nullptr, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);
	auto psCode = compileShader("terrainDeferred.hlsl", nullptr, "PSMain", "ps_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

	// 5-slot input layout: Position (slot 0), Normal (slot 1), Tangent (slot 2), Bitangent (slot 3), UV (slot 4)
	auto elemDescs = std::vector<D3D12_INPUT_ELEMENT_DESC>{
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "POSITION", .SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT, .InputSlot = 0u,
			.AlignedByteOffset = 0u, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "NORMAL", .SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT, .InputSlot = 1u,
			.AlignedByteOffset = 0u, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "TANGENT", .SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT, .InputSlot = 2u,
			.AlignedByteOffset = 0u, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "BITANGENT", .SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT, .InputSlot = 3u,
			.AlignedByteOffset = 0u, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0u
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "UV", .SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32_FLOAT, .InputSlot = 4u,
			.AlignedByteOffset = 0u, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0u
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
		.BlendState = D3D12_BLEND_DESC{
			.AlphaToCoverageEnable = false,
			.IndependentBlendEnable = false
		},
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
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
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.SampleDesc = DXGI_SAMPLE_DESC{.Count = 1u, .Quality = 0u},
		.NodeMask = 0u,
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	// 4 GBuffer RTVs (matches GBufferData layout in sharedResources.hpp)
	psoDesc.NumRenderTargets = 4u;
	for (int i = 0; i < 4; ++i) {
		psoDesc.BlendState.RenderTarget[i].BlendEnable = false;
		psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;  // Albedo.rgb + AO.a
	psoDesc.RTVFormats[1] = DXGI_FORMAT_R16G16_FLOAT;    // NormalV oct-encoded
	psoDesc.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM;  // LightAccum.rgb + Roughness.a
	psoDesc.RTVFormats[3] = DXGI_FORMAT_R8_UNORM;        // Metallic
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);

	setD3DName(ret.Get(), "TerrainDeferredGBufferShader");

	return ret;
}

ComPtr<ID3D12PipelineState> createTerrainShadowMapShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	auto vsCode = compileShader("terrainShadowMap.hlsl", nullptr, "VSMain", "vs_6_0",
		D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

	// Position only (slot 0) - matches TerrainPipeline VBV cache index 0
	auto elemDescs = std::vector<D3D12_INPUT_ELEMENT_DESC>{
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName         = "POSITION",
			.SemanticIndex        = 0u,
			.Format               = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot            = 0u,
			.AlignedByteOffset    = 0u,
			.InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		}
	};

	auto inputLayoutDesc = D3D12_INPUT_LAYOUT_DESC{
		.pInputElementDescs = elemDescs.data(),
		.NumElements        = static_cast<UINT>(elemDescs.size())
	};

	auto psoDesc = D3D12_GRAPHICS_PIPELINE_STATE_DESC{
		.pRootSignature = rootSig,
		.VS             = vsCode.byteCode,
		// No PS: depth-only pass
		.BlendState = D3D12_BLEND_DESC{
			.AlphaToCoverageEnable  = false,
			.IndependentBlendEnable = false
		},
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
		.RasterizerState = D3D12_RASTERIZER_DESC{
			.FillMode              = D3D12_FILL_MODE_SOLID,
			.CullMode              = D3D12_CULL_MODE_BACK,
			.FrontCounterClockwise = false,
			.DepthBias             = 1000,
			.DepthBiasClamp        = 0.01f,
			.SlopeScaledDepthBias  = 2.5f,
			.DepthClipEnable       = true,
			.MultisampleEnable     = false,
			.AntialiasedLineEnable = false,
			.ForcedSampleCount     = 0u
		},
		.DepthStencilState = D3D12_DEPTH_STENCIL_DESC{
			.DepthEnable      = true,
			.DepthWriteMask   = D3D12_DEPTH_WRITE_MASK_ALL,
			.DepthFunc        = D3D12_COMPARISON_FUNC_LESS,
			.StencilEnable    = false,
			.StencilReadMask  = 0u,
			.StencilWriteMask = 0u,
			.FrontFace        = D3D12_DEPTH_STENCILOP_DESC{},
			.BackFace         = D3D12_DEPTH_STENCILOP_DESC{}
		},
		.InputLayout           = inputLayoutDesc,
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.SampleDesc            = DXGI_SAMPLE_DESC{.Count = 1u, .Quality = 0u},
		.NodeMask              = 0u,
		.Flags                 = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	// Depth-only: zero render targets
	psoDesc.NumRenderTargets = 0u;
	psoDesc.DSVFormat        = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);

	setD3DName(ret.Get(), "TerrainShadowMapShader");

	return ret;
}

ComPtr<ID3D12PipelineState> createTerrainShadowMapCSMShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	auto vsCode = compileShader("terrainShadowMapCSM.hlsl", nullptr, "VSMain", "vs_6_0",
		D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

	// Position only (slot 0) - matches TerrainPipeline VBV cache index 0
	auto elemDescs = std::vector<D3D12_INPUT_ELEMENT_DESC>{
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName         = "POSITION",
			.SemanticIndex        = 0u,
			.Format               = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot            = 0u,
			.AlignedByteOffset    = 0u,
			.InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0u
		}
	};

	auto inputLayoutDesc = D3D12_INPUT_LAYOUT_DESC{
		.pInputElementDescs = elemDescs.data(),
		.NumElements        = static_cast<UINT>(elemDescs.size())
	};

	auto psoDesc = D3D12_GRAPHICS_PIPELINE_STATE_DESC{
		.pRootSignature = rootSig,
		.VS             = vsCode.byteCode,
		// No PS: depth-only pass
		.BlendState = D3D12_BLEND_DESC{
			.AlphaToCoverageEnable  = false,
			.IndependentBlendEnable = false
		},
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
		.RasterizerState = D3D12_RASTERIZER_DESC{
			.FillMode              = D3D12_FILL_MODE_SOLID,
			.CullMode              = D3D12_CULL_MODE_BACK,
			.FrontCounterClockwise = false,
			.DepthBias             = 1000,
			.DepthBiasClamp        = 0.01f,
			.SlopeScaledDepthBias  = 2.5f,
			.DepthClipEnable       = true,
			.MultisampleEnable     = false,
			.AntialiasedLineEnable = false,
			.ForcedSampleCount     = 0u
		},
		.DepthStencilState = D3D12_DEPTH_STENCIL_DESC{
			.DepthEnable      = true,
			.DepthWriteMask   = D3D12_DEPTH_WRITE_MASK_ALL,
			.DepthFunc        = D3D12_COMPARISON_FUNC_LESS,
			.StencilEnable    = false,
			.StencilReadMask  = 0u,
			.StencilWriteMask = 0u,
			.FrontFace        = D3D12_DEPTH_STENCILOP_DESC{},
			.BackFace         = D3D12_DEPTH_STENCILOP_DESC{}
		},
		.InputLayout           = inputLayoutDesc,
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.SampleDesc            = DXGI_SAMPLE_DESC{.Count = 1u, .Quality = 0u},
		.NodeMask              = 0u,
		.Flags                 = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	// Depth-only: zero render targets
	psoDesc.NumRenderTargets = 0u;
	psoDesc.DSVFormat        = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);

	setD3DName(ret.Get(), "TerrainShadowMapCSMShader");

	return ret;
}

// Builds the PSO used by BillboardPipeline. Same HLSL for all modes; only the
// blend state differs between them.
static ComPtr<ID3D12PipelineState> createBillboardShaderImpl(
	ID3D12Device* device, ID3D12RootSignature* rootSig, ps::BlendMode mode
) {
	ComPtr<ID3D12PipelineState> ret{};

	auto vsCode = compileShader( "billboard.hlsl", nullptr, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u );
	auto psCode = compileShader( "billboard.hlsl", nullptr, "PSMain", "ps_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u );
	auto gsCode = compileShader( "billboard.hlsl", nullptr, "GSMain", "gs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u );

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
			.SemanticName = "SIZE",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32_FLOAT,
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
		.GS = gsCode.byteCode,
		.BlendState = D3D12_BLEND_DESC{
			.AlphaToCoverageEnable = false,
			.IndependentBlendEnable = false
		},
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
		.RasterizerState = D3D12_RASTERIZER_DESC{
			.FillMode = D3D12_FILL_MODE_SOLID,
			.CullMode = D3D12_CULL_MODE_NONE,
			.FrontCounterClockwise = false,
			.DepthBias = 0,
			.DepthBiasClamp = 0.f,
			.SlopeScaledDepthBias = 0.f,
			.DepthClipEnable = true,
			.MultisampleEnable = false,
			.AntialiasedLineEnable = false,
			.ForcedSampleCount = 0u
		},
		// Transparent particles: depth-test only, no depth write.
		.DepthStencilState = D3D12_DEPTH_STENCIL_DESC{
			.DepthEnable = true,
			.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
			.DepthFunc = D3D12_COMPARISON_FUNC_LESS,
			.StencilEnable = false,
			.StencilReadMask = 0u,
			.StencilWriteMask = 0u,
			.FrontFace = D3D12_DEPTH_STENCILOP_DESC{},
			.BackFace = D3D12_DEPTH_STENCILOP_DESC{}
		},
		.InputLayout = inputLayoutDesc,
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,
		.SampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
		.NodeMask = 0u,
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	psoDesc.NumRenderTargets = 1u;
	psoDesc.BlendState.RenderTarget[0].BlendEnable = true;

	switch (mode) {
	case ps::BlendMode::Additive:
		// src * 1 + dst * 1
		psoDesc.BlendState.RenderTarget[0].SrcBlend  = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		break;
	case ps::BlendMode::Multiply:
		// src.rgb * dst  (white = transparent, black = fully dark)
		psoDesc.BlendState.RenderTarget[0].SrcBlend  = D3D12_BLEND_DEST_COLOR;
		psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
		break;
	case ps::BlendMode::PremultipliedAlpha:
		// src.rgb + dst * (1 - src.a)  — additive-baked textures
		psoDesc.BlendState.RenderTarget[0].SrcBlend  = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		break;
	case ps::BlendMode::Alpha:
	default:
		// src.rgb * src.a + dst * (1 - src.a)
		psoDesc.BlendState.RenderTarget[0].SrcBlend  = D3D12_BLEND_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		break;
	}

	psoDesc.BlendState.RenderTarget[0].BlendOp        = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha  = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	psoDesc.BlendState.RenderTarget[0].BlendOpAlpha   = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat     = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState( &psoDesc, __uuidof(ID3D12PipelineState), &ret ),
		false
	);

	static constexpr const char* kNames[] = {
		"BillboardShader", "BillboardShaderAdditive",
		"BillboardShaderMultiply", "BillboardShaderPremultiplied"
	};
	setD3DName( ret.Get(), kNames[static_cast<int>(mode)] );

	return ret;
}

ComPtr<ID3D12PipelineState> createBillboardShader( ID3D12Device* device, ID3D12RootSignature* rootSig ) {
	return createBillboardShaderImpl( device, rootSig, ps::BlendMode::Alpha );
}

ComPtr<ID3D12PipelineState> createBillboardShaderAdditive( ID3D12Device* device, ID3D12RootSignature* rootSig ) {
	return createBillboardShaderImpl( device, rootSig, ps::BlendMode::Additive );
}

ComPtr<ID3D12PipelineState> createBillboardShaderMultiply( ID3D12Device* device, ID3D12RootSignature* rootSig ) {
	return createBillboardShaderImpl( device, rootSig, ps::BlendMode::Multiply );
}

ComPtr<ID3D12PipelineState> createBillboardShaderPremultiplied( ID3D12Device* device, ID3D12RootSignature* rootSig ) {
	return createBillboardShaderImpl( device, rootSig, ps::BlendMode::PremultipliedAlpha );
}

ComPtr<ID3D12PipelineState> createMeshParticleShader( ID3D12Device* device, ID3D12RootSignature* rootSig ) {
	ComPtr<ID3D12PipelineState> ret{};

	auto vsCode = compileShader( "meshParticle.hlsl", nullptr, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u );
	auto psCode = compileShader( "meshParticle.hlsl", nullptr, "PSMain", "ps_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u );

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
		.BlendState = D3D12_BLEND_DESC{
			.AlphaToCoverageEnable = false,
			.IndependentBlendEnable = false
		},
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
		.RasterizerState = D3D12_RASTERIZER_DESC{
			.FillMode = D3D12_FILL_MODE_SOLID,
			.CullMode = D3D12_CULL_MODE_NONE,  // double-sided: slash mesh has no back-face
			.FrontCounterClockwise = false,
			.DepthBias = 0,
			.DepthBiasClamp = 0.f,
			.SlopeScaledDepthBias = 0.f,
			.DepthClipEnable = true,
			.MultisampleEnable = false,
			.AntialiasedLineEnable = false,
			.ForcedSampleCount = 0u
		},
		.DepthStencilState = D3D12_DEPTH_STENCIL_DESC{
			.DepthEnable = true,
			.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
			.DepthFunc = D3D12_COMPARISON_FUNC_LESS,
			.StencilEnable = false,
			.StencilReadMask = 0u,
			.StencilWriteMask = 0u,
			.FrontFace = D3D12_DEPTH_STENCILOP_DESC{},
			.BackFace = D3D12_DEPTH_STENCILOP_DESC{}
		},
		.InputLayout = inputLayoutDesc,
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.SampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
		.NodeMask = 0u,
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	// Standard alpha blend: result = src.rgb * src.a + dst.rgb * (1 - src.a)
	psoDesc.NumRenderTargets = 1u;
	psoDesc.BlendState.RenderTarget[0].BlendEnable = true;
	psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState( &psoDesc, __uuidof(ID3D12PipelineState), &ret ),
		false
	);

	setD3DName( ret.Get(), "MeshParticleShader" );

	return ret;
}

ComPtr<ID3D12PipelineState> createSmokeBlendCGShader( ID3D12Device* device, ID3D12RootSignature* rootSig ) {
	ComPtr<ID3D12PipelineState> ret{};

	auto vsCode = compileShader( "smokeBlendCG.hlsl", nullptr, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u );
	auto psCode = compileShader( "smokeBlendCG.hlsl", nullptr, "PSMain", "ps_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u );
	auto gsCode = compileShader( "smokeBlendCG.hlsl", nullptr, "GSMain", "gs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u );

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
			.SemanticName = "SIZE",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32_FLOAT,
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
		.GS = gsCode.byteCode,
		.BlendState = D3D12_BLEND_DESC{
			.AlphaToCoverageEnable = false,
			.IndependentBlendEnable = false
		},
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
		.RasterizerState = D3D12_RASTERIZER_DESC{
			.FillMode = D3D12_FILL_MODE_SOLID,
			.CullMode = D3D12_CULL_MODE_NONE,
			.FrontCounterClockwise = false,
			.DepthBias = 0,
			.DepthBiasClamp = 0.f,
			.SlopeScaledDepthBias = 0.f,
			.DepthClipEnable = true,
			.MultisampleEnable = false,
			.AntialiasedLineEnable = false,
			.ForcedSampleCount = 0u
		},
		.DepthStencilState = D3D12_DEPTH_STENCIL_DESC{
			.DepthEnable = true,
			.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
			.DepthFunc = D3D12_COMPARISON_FUNC_LESS,
			.StencilEnable = false,
			.StencilReadMask = 0u,
			.StencilWriteMask = 0u,
			.FrontFace = D3D12_DEPTH_STENCILOP_DESC{},
			.BackFace = D3D12_DEPTH_STENCILOP_DESC{}
		},
		.InputLayout = inputLayoutDesc,
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,
		.SampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
		.NodeMask = 0u,
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	psoDesc.NumRenderTargets = 1u;
	psoDesc.BlendState.RenderTarget[0].BlendEnable = true;
	psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState( &psoDesc, __uuidof(ID3D12PipelineState), &ret ),
		false
	);

	setD3DName( ret.Get(), "SmokeBlendCGShader" );

	return ret;
}

ComPtr<ID3D12PipelineState> createBlendCGMeshShader( ID3D12Device* device, ID3D12RootSignature* rootSig ) {
	ComPtr<ID3D12PipelineState> ret{};

	auto vsCode = compileShader( "blendCGMesh.hlsl", nullptr, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u );
	auto psCode = compileShader( "blendCGMesh.hlsl", nullptr, "PSMain", "ps_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u );

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
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "COLOR",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
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
		.BlendState = D3D12_BLEND_DESC{
			.AlphaToCoverageEnable = false,
			.IndependentBlendEnable = false
		},
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
		.RasterizerState = D3D12_RASTERIZER_DESC{
			.FillMode = D3D12_FILL_MODE_SOLID,
			.CullMode = D3D12_CULL_MODE_NONE,
			.FrontCounterClockwise = false,
			.DepthBias = 0,
			.DepthBiasClamp = 0.f,
			.SlopeScaledDepthBias = 0.f,
			.DepthClipEnable = true,
			.MultisampleEnable = false,
			.AntialiasedLineEnable = false,
			.ForcedSampleCount = 0u
		},
		.DepthStencilState = D3D12_DEPTH_STENCIL_DESC{
			.DepthEnable = true,
			.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
			.DepthFunc = D3D12_COMPARISON_FUNC_LESS,
			.StencilEnable = false,
			.StencilReadMask = 0u,
			.StencilWriteMask = 0u,
			.FrontFace = D3D12_DEPTH_STENCILOP_DESC{},
			.BackFace = D3D12_DEPTH_STENCILOP_DESC{}
		},
		.InputLayout = inputLayoutDesc,
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.SampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
		.NodeMask = 0u,
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	psoDesc.NumRenderTargets = 1u;
	psoDesc.BlendState.RenderTarget[0].BlendEnable = true;
	psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState( &psoDesc, __uuidof(ID3D12PipelineState), &ret ),
		false
	);

	setD3DName( ret.Get(), "BlendCGMeshShader" );

	return ret;
}

ComPtr<ID3D12PipelineState> createSwordSlashShader( ID3D12Device* device, ID3D12RootSignature* rootSig ) {
	ComPtr<ID3D12PipelineState> ret{};

	auto vsCode = compileShader( "swordSlash.hlsl", nullptr, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u );
	auto psCode = compileShader( "swordSlash.hlsl", nullptr, "PSMain", "ps_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u );

	// Input layout: Slot0=POSITION(float3), Slot1=UV(float2), Slot2=COLOR(float4)
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
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "COLOR",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
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
		.BlendState = D3D12_BLEND_DESC{
			.AlphaToCoverageEnable = false,
			.IndependentBlendEnable = false
		},
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
		.RasterizerState = D3D12_RASTERIZER_DESC{
			.FillMode = D3D12_FILL_MODE_SOLID,
			.CullMode = D3D12_CULL_MODE_NONE,
			.FrontCounterClockwise = false,
			.DepthBias = 0,
			.DepthBiasClamp = 0.f,
			.SlopeScaledDepthBias = 0.f,
			.DepthClipEnable = true,
			.MultisampleEnable = false,
			.AntialiasedLineEnable = false,
			.ForcedSampleCount = 0u
		},
		.DepthStencilState = D3D12_DEPTH_STENCIL_DESC{
			.DepthEnable = true,
			.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
			.DepthFunc = D3D12_COMPARISON_FUNC_LESS,
			.StencilEnable = false,
			.StencilReadMask = 0u,
			.StencilWriteMask = 0u,
			.FrontFace = D3D12_DEPTH_STENCILOP_DESC{},
			.BackFace = D3D12_DEPTH_STENCILOP_DESC{}
		},
		.InputLayout = inputLayoutDesc,
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.SampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
		.NodeMask = 0u,
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	// Standard alpha blend: src.rgb * src.a + dst.rgb * (1 - src.a)
	psoDesc.NumRenderTargets = 1u;
	psoDesc.BlendState.RenderTarget[0].BlendEnable = true;
	psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState( &psoDesc, __uuidof(ID3D12PipelineState), &ret ),
		false
	);

	setD3DName( ret.Get(), "SwordSlashShader" );

	return ret;
}

ComPtr<ID3D12PipelineState> createTwoSidesShader( ID3D12Device* device, ID3D12RootSignature* rootSig ) {
	ComPtr<ID3D12PipelineState> ret{};

	auto vsCode = compileShader( "twoSides.hlsl", nullptr, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u );
	auto psCode = compileShader( "twoSides.hlsl", nullptr, "PSMain", "ps_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u );

	// Input layout: Slot0=POSITION(float3), Slot1=NORMAL(float3), Slot2=UV(float2), Slot3=COLOR(float4)
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
		},
		D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "COLOR",
			.SemanticIndex = 0u,
			.Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
			.InputSlot = 3u,
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
		.BlendState = D3D12_BLEND_DESC{
			.AlphaToCoverageEnable = false,
			.IndependentBlendEnable = false
		},
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
		.RasterizerState = D3D12_RASTERIZER_DESC{
			.FillMode = D3D12_FILL_MODE_SOLID,
			.CullMode = D3D12_CULL_MODE_NONE,  // two-sided
			.FrontCounterClockwise = false,
			.DepthBias = 0,
			.DepthBiasClamp = 0.f,
			.SlopeScaledDepthBias = 0.f,
			.DepthClipEnable = true,
			.MultisampleEnable = false,
			.AntialiasedLineEnable = false,
			.ForcedSampleCount = 0u
		},
		.DepthStencilState = D3D12_DEPTH_STENCIL_DESC{
			.DepthEnable = true,
			.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
			.DepthFunc = D3D12_COMPARISON_FUNC_LESS,
			.StencilEnable = false,
			.StencilReadMask = 0u,
			.StencilWriteMask = 0u,
			.FrontFace = D3D12_DEPTH_STENCILOP_DESC{},
			.BackFace = D3D12_DEPTH_STENCILOP_DESC{}
		},
		.InputLayout = inputLayoutDesc,
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.SampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
		.NodeMask = 0u,
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	// Standard alpha blend
	psoDesc.NumRenderTargets = 1u;
	psoDesc.BlendState.RenderTarget[0].BlendEnable = true;
	psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState( &psoDesc, __uuidof(ID3D12PipelineState), &ret ),
		false
	);

	setD3DName( ret.Get(), "TwoSidesShader" );

	return ret;
}

ComPtr<ID3D12PipelineState> createHiZOccluderShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	// 셰이더 컴파일
	auto vsCode = compileShader("hiZOccluder.hlsl", nullptr, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

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
		}
	};

	auto inputLayoutDesc = D3D12_INPUT_LAYOUT_DESC{
		.pInputElementDescs = elemDescs.data(),
		.NumElements = static_cast<UINT>(elemDescs.size())
	};

	auto psoDesc = D3D12_GRAPHICS_PIPELINE_STATE_DESC{
		.pRootSignature = rootSig,
		.VS = vsCode.byteCode,
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

	// Depth-only: 렌더 타겟 없음
	psoDesc.NumRenderTargets = 0u;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);

	setD3DName(ret.Get(), "HiZOccluderShader");

	return ret;
}

ComPtr<ID3D12PipelineState> createHiZMapShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	// 셰이더 컴파일
	auto csCode = compileShader("hiZMap.hlsl", nullptr, "CSMain", "cs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

	auto psoDesc = D3D12_COMPUTE_PIPELINE_STATE_DESC {
		.pRootSignature = rootSig,
		.CS = csCode.byteCode,
		.NodeMask = 0u,
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	DISPLAY_ERROR_DX_HR(
		device->CreateComputePipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);

	setD3DName(ret.Get(), "HiZMapShader");

	return ret;
}

ComPtr<ID3D12PipelineState> createHiZClearShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	auto csCode = compileShader("hiZClear.hlsl", nullptr, "CSMain", "cs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

	auto psoDesc = D3D12_COMPUTE_PIPELINE_STATE_DESC {
		.pRootSignature = rootSig,
		.CS = csCode.byteCode,
		.NodeMask = 0u,
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	DISPLAY_ERROR_DX_HR(
		device->CreateComputePipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);

	setD3DName(ret.Get(), "HiZClearShader");

	return ret;
}

ComPtr<ID3D12PipelineState> createHiZCullShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	// 셰이더 컴파일
	auto csCode = compileShader("hiZCull.hlsl", nullptr, "CSMain", "cs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

	auto psoDesc = D3D12_COMPUTE_PIPELINE_STATE_DESC {
		.pRootSignature = rootSig,
		.CS = csCode.byteCode,
		.NodeMask = 0u,
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	DISPLAY_ERROR_DX_HR(
		device->CreateComputePipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);

	setD3DName(ret.Get(), "HiZCullShader");

	return ret;
}

ComPtr<ID3D12PipelineState> createHiZCompactShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	// 셰이더 컴파일
	auto csCode = compileShader("hiZCompact.hlsl", nullptr, "CSMain", "cs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

	auto psoDesc = D3D12_COMPUTE_PIPELINE_STATE_DESC {
		.pRootSignature = rootSig,
		.CS = csCode.byteCode,
		.NodeMask = 0u,
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	DISPLAY_ERROR_DX_HR(
		device->CreateComputePipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);

	setD3DName(ret.Get(), "HiZCompactShader");

	return ret;
}

ComPtr<ID3D12PipelineState> createHiZCommandShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	// 셰이더 컴파일
	auto csCode = compileShader("hiZCommand.hlsl", nullptr, "CSMain", "cs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

	auto psoDesc = D3D12_COMPUTE_PIPELINE_STATE_DESC {
		.pRootSignature = rootSig,
		.CS = csCode.byteCode,
		.NodeMask = 0u,
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	DISPLAY_ERROR_DX_HR(
		device->CreateComputePipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);

	setD3DName(ret.Get(), "HiZCommandShader");

	return ret;
}

ComPtr<ID3D12PipelineState> createPrefixSumShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	// 셰이더 컴파일
	auto csCode = compileShader("prefixSum.hlsl", nullptr, "CSMain", "cs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

	auto psoDesc = D3D12_COMPUTE_PIPELINE_STATE_DESC {
		.pRootSignature = rootSig,
		.CS = csCode.byteCode,
		.NodeMask = 0u,
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	DISPLAY_ERROR_DX_HR(
		device->CreateComputePipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);

	setD3DName(ret.Get(), "PrefixSumShader");

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

	// b0, space1: FirstInstanceOffset
	addParam( "FirstInstanceOffset", idxRootParam++, D3D12_ROOT_PARAMETER{
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
		.Constants = D3D12_ROOT_CONSTANTS {
			.ShaderRegister = 0u,
			.RegisterSpace = 1u,
			.Num32BitValues = 1u
		},
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	} );

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

	// t0, space1: srcTex
	const auto srcTexRange = D3D12_DESCRIPTOR_RANGE {
		.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		.NumDescriptors = 1u,
		.BaseShaderRegister = 0u,
		.RegisterSpace = 1u,
		.OffsetInDescriptorsFromTableStart = 0u
	};

	addParam( "SrcTex", idxRootParam++, D3D12_ROOT_PARAMETER{
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
		.DescriptorTable = D3D12_ROOT_DESCRIPTOR_TABLE{
			.NumDescriptorRanges = 1u,
			.pDescriptorRanges = &srcTexRange
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

	// t2: BoneData
	addParam( "BoneData", idxRootParam++, D3D12_ROOT_PARAMETER{
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
		.Descriptor = D3D12_ROOT_DESCRIPTOR{
			.ShaderRegister = 2u,
			.RegisterSpace = 0u
		},
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	} );

	// t3: SrcCnts0
	addParam( "SrcCnts0", idxRootParam++, D3D12_ROOT_PARAMETER{
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
		.Descriptor = D3D12_ROOT_DESCRIPTOR{
			.ShaderRegister = 3u,
			.RegisterSpace = 0u
		},
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	} );

	// t4: SrcCnts1
	addParam( "SrcCnts1", idxRootParam++, D3D12_ROOT_PARAMETER{
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
		.Descriptor = D3D12_ROOT_DESCRIPTOR{
			.ShaderRegister = 4u,
			.RegisterSpace = 0u
		},
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	} );

	// t5: PerGroupData
	addParam( "PerGroupData", idxRootParam++, D3D12_ROOT_PARAMETER{
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV,
		.Descriptor = D3D12_ROOT_DESCRIPTOR{
			.ShaderRegister = 5u,
			.RegisterSpace = 0u
		},
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	} );

	// u0: DestTex
	const auto destTexRange = D3D12_DESCRIPTOR_RANGE {
		.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
		.NumDescriptors = 1u,
		.BaseShaderRegister = 0u,
		.RegisterSpace = 0u,
		.OffsetInDescriptorsFromTableStart = 0u
	};

	addParam( "DestTex", idxRootParam++, D3D12_ROOT_PARAMETER{
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
		.DescriptorTable = D3D12_ROOT_DESCRIPTOR_TABLE{
			.NumDescriptorRanges = 1u,
			.pDescriptorRanges = &destTexRange
		},
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	} );

	// u1: DestCnts0
	addParam( "DestCnts0", idxRootParam++, D3D12_ROOT_PARAMETER{
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV,
		.Descriptor = D3D12_ROOT_DESCRIPTOR{
			.ShaderRegister = 1u,
			.RegisterSpace = 0u
		},
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	} );

	// u2: DestCnts1
	addParam( "DestCnts1", idxRootParam++, D3D12_ROOT_PARAMETER{
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV,
		.Descriptor = D3D12_ROOT_DESCRIPTOR{
			.ShaderRegister = 2u,
			.RegisterSpace = 0u
		},
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	} );

	// u3: OutPerGroupData
	addParam( "OutPerGroupData", idxRootParam++, D3D12_ROOT_PARAMETER{
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV,
		.Descriptor = D3D12_ROOT_DESCRIPTOR{
			.ShaderRegister = 3u,
			.RegisterSpace = 0u
		},
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	} );

	// u0, space1: IndirectCommand
	addParam( "IndirectCommand", idxRootParam++, D3D12_ROOT_PARAMETER{
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV,
		.Descriptor = D3D12_ROOT_DESCRIPTOR{
			.ShaderRegister = 0u,
			.RegisterSpace = 1u
		},
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
	} );

	// ==== bindless 환경을 고려한 루트 파라미터들 =========
	const auto tex2dRange = D3D12_DESCRIPTOR_RANGE {
		.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		.NumDescriptors = UINT(-1),
		.BaseShaderRegister = 10u,
		.RegisterSpace = 1u,
		.OffsetInDescriptorsFromTableStart = 0u
	};

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

void CmdSig::build(ID3D12Device* device, const RootSig& root) {
	auto args = std::vector<D3D12_INDIRECT_ARGUMENT_DESC>();
	auto out = std::back_inserter(args);

	// 1) Root Constant — groupOffset 1개 전달
	out = D3D12_INDIRECT_ARGUMENT_DESC{
		.Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT,
		.Constant = {
			.RootParameterIndex = root.paramIdx("FirstInstanceOffset"),
			.DestOffsetIn32BitValues = 0u,
			.Num32BitValuesToSet = 1u
		}
	};

	// 2) DrawIndexedInstanced
	out = D3D12_INDIRECT_ARGUMENT_DESC{
		.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED
	};

	auto csDesc = D3D12_COMMAND_SIGNATURE_DESC{
		.ByteStride = sizeof(u32t) + sizeof(DrawIndexedInstancedArgs),
		.NumArgumentDescs = 2u,
		.pArgumentDescs = args.data(),
		.NodeMask = 0u
	};

	DISPLAY_ERROR_DX_HR( device->CreateCommandSignature(
		&csDesc, root.get(), __uuidof(ID3D12CommandSignature), &cmdSig_
	), false );

	// 이름 설정
	name_ = "CommandSignature";
	setD3DName(cmdSig_.Get(), name_);
}


ComPtr<ID3D12PipelineState> createPBRDeferredGBufferShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	auto vsCode = compileShader("pbrDeferred.hlsl", nullptr, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);
	auto psCode = compileShader("pbrDeferred.hlsl", nullptr, "PSMain", "ps_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

	auto elemDescs = std::vector<D3D12_INPUT_ELEMENT_DESC>{
		D3D12_INPUT_ELEMENT_DESC{ .SemanticName = "POSITION",  .SemanticIndex = 0u, .Format = DXGI_FORMAT_R32G32B32_FLOAT,    .InputSlot = 0u, .AlignedByteOffset = 0u, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0u },
		D3D12_INPUT_ELEMENT_DESC{ .SemanticName = "NORMAL",    .SemanticIndex = 0u, .Format = DXGI_FORMAT_R32G32B32_FLOAT,    .InputSlot = 1u, .AlignedByteOffset = 0u, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0u },
		D3D12_INPUT_ELEMENT_DESC{ .SemanticName = "TANGENT",   .SemanticIndex = 0u, .Format = DXGI_FORMAT_R32G32B32_FLOAT,    .InputSlot = 2u, .AlignedByteOffset = 0u, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0u },
		D3D12_INPUT_ELEMENT_DESC{ .SemanticName = "BITANGENT", .SemanticIndex = 0u, .Format = DXGI_FORMAT_R32G32B32_FLOAT,    .InputSlot = 3u, .AlignedByteOffset = 0u, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0u },
		D3D12_INPUT_ELEMENT_DESC{ .SemanticName = "UV",        .SemanticIndex = 0u, .Format = DXGI_FORMAT_R32G32_FLOAT,        .InputSlot = 4u, .AlignedByteOffset = 0u, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0u }
	};

	auto inputLayoutDesc = D3D12_INPUT_LAYOUT_DESC{
		.pInputElementDescs = elemDescs.data(),
		.NumElements        = static_cast<UINT>(elemDescs.size())
	};

	auto psoDesc = D3D12_GRAPHICS_PIPELINE_STATE_DESC{
		.pRootSignature = rootSig,
		.VS = vsCode.byteCode,
		.PS = psCode.byteCode,
		.BlendState = D3D12_BLEND_DESC{ .AlphaToCoverageEnable = false, .IndependentBlendEnable = false },
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
		.RasterizerState = D3D12_RASTERIZER_DESC{
			.FillMode              = D3D12_FILL_MODE_SOLID,
			.CullMode              = D3D12_CULL_MODE_BACK,
			.FrontCounterClockwise = false,
			.DepthBias             = 0,
			.DepthBiasClamp        = 0.f,
			.SlopeScaledDepthBias  = 0.f,
			.DepthClipEnable       = true,
			.MultisampleEnable     = false,
			.AntialiasedLineEnable = false,
			.ForcedSampleCount     = 0u
		},
		.DepthStencilState = D3D12_DEPTH_STENCIL_DESC{
			.DepthEnable      = true,
			.DepthWriteMask   = D3D12_DEPTH_WRITE_MASK_ALL,
			.DepthFunc        = D3D12_COMPARISON_FUNC_LESS,
			.StencilEnable    = false,
			.StencilReadMask  = 0u,
			.StencilWriteMask = 0u,
			.FrontFace        = D3D12_DEPTH_STENCILOP_DESC{},
			.BackFace         = D3D12_DEPTH_STENCILOP_DESC{}
		},
		.InputLayout            = inputLayoutDesc,
		.PrimitiveTopologyType  = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.SampleDesc             = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
		.NodeMask               = 0u,
		.Flags                  = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	// 4 GBuffer render targets
	psoDesc.NumRenderTargets = 4u;
	for (UINT i = 0u; i < 4u; ++i) {
		psoDesc.BlendState.RenderTarget[i].BlendEnable           = false;
		psoDesc.BlendState.RenderTarget[i].SrcBlend              = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[i].DestBlend             = D3D12_BLEND_ZERO;
		psoDesc.BlendState.RenderTarget[i].BlendOp               = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha         = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[i].DestBlendAlpha        = D3D12_BLEND_ZERO;
		psoDesc.BlendState.RenderTarget[i].BlendOpAlpha          = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;  // GB0
	psoDesc.RTVFormats[1] = DXGI_FORMAT_R16G16_FLOAT;     // GB1
	psoDesc.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM;  // GB2
	psoDesc.RTVFormats[3] = DXGI_FORMAT_R8_UNORM;         // GB3
	psoDesc.DSVFormat     = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);
	setD3DName(ret.Get(), "PBRDeferredGBufferShader");
	return ret;
}

ComPtr<ID3D12PipelineState> createPBRDeferredSkinnedIndirectGBufferShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	static const D3D_SHADER_MACRO hiZCullDefines[] = { { "HiZCull", "1" }, { nullptr, nullptr } };
	auto vsCode = compileShader("pbrDeferredSkinned.hlsl", hiZCullDefines, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);
	auto psCode = compileShader("pbrDeferredSkinned.hlsl", hiZCullDefines, "PSMain", "ps_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

	auto elemDescs = std::vector<D3D12_INPUT_ELEMENT_DESC>{
		D3D12_INPUT_ELEMENT_DESC{ .SemanticName = "POSITION",     .SemanticIndex = 0u, .Format = DXGI_FORMAT_R32G32B32_FLOAT,       .InputSlot = 0u, .AlignedByteOffset = 0u, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0u },
		D3D12_INPUT_ELEMENT_DESC{ .SemanticName = "NORMAL",       .SemanticIndex = 0u, .Format = DXGI_FORMAT_R32G32B32_FLOAT,       .InputSlot = 1u, .AlignedByteOffset = 0u, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0u },
		D3D12_INPUT_ELEMENT_DESC{ .SemanticName = "TANGENT",      .SemanticIndex = 0u, .Format = DXGI_FORMAT_R32G32B32_FLOAT,       .InputSlot = 2u, .AlignedByteOffset = 0u, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0u },
		D3D12_INPUT_ELEMENT_DESC{ .SemanticName = "BITANGENT",    .SemanticIndex = 0u, .Format = DXGI_FORMAT_R32G32B32_FLOAT,       .InputSlot = 3u, .AlignedByteOffset = 0u, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0u },
		D3D12_INPUT_ELEMENT_DESC{ .SemanticName = "UV",           .SemanticIndex = 0u, .Format = DXGI_FORMAT_R32G32_FLOAT,           .InputSlot = 4u, .AlignedByteOffset = 0u, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0u },
		D3D12_INPUT_ELEMENT_DESC{ .SemanticName = "BONE_INDICES", .SemanticIndex = 0u, .Format = DXGI_FORMAT_R32G32B32A32_SINT,     .InputSlot = 5u, .AlignedByteOffset = 0u, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0u },
		D3D12_INPUT_ELEMENT_DESC{ .SemanticName = "BONE_WEIGHTS", .SemanticIndex = 0u, .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,    .InputSlot = 6u, .AlignedByteOffset = 0u, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0u }
	};

	auto inputLayoutDesc = D3D12_INPUT_LAYOUT_DESC{
		.pInputElementDescs = elemDescs.data(),
		.NumElements        = static_cast<UINT>(elemDescs.size())
	};

	auto psoDesc = D3D12_GRAPHICS_PIPELINE_STATE_DESC{
		.pRootSignature = rootSig,
		.VS = vsCode.byteCode,
		.PS = psCode.byteCode,
		.BlendState = D3D12_BLEND_DESC{ .AlphaToCoverageEnable = false, .IndependentBlendEnable = false },
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
		.RasterizerState = D3D12_RASTERIZER_DESC{
			.FillMode              = D3D12_FILL_MODE_SOLID,
			.CullMode              = D3D12_CULL_MODE_BACK,
			.FrontCounterClockwise = false,
			.DepthBias             = 0,
			.DepthBiasClamp        = 0.f,
			.SlopeScaledDepthBias  = 0.f,
			.DepthClipEnable       = true,
			.MultisampleEnable     = false,
			.AntialiasedLineEnable = false,
			.ForcedSampleCount     = 0u
		},
		.DepthStencilState = D3D12_DEPTH_STENCIL_DESC{
			.DepthEnable      = true,
			.DepthWriteMask   = D3D12_DEPTH_WRITE_MASK_ALL,
			.DepthFunc        = D3D12_COMPARISON_FUNC_LESS,
			.StencilEnable    = false,
			.StencilReadMask  = 0u,
			.StencilWriteMask = 0u,
			.FrontFace        = D3D12_DEPTH_STENCILOP_DESC{},
			.BackFace         = D3D12_DEPTH_STENCILOP_DESC{}
		},
		.InputLayout           = inputLayoutDesc,
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.SampleDesc            = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
		.NodeMask              = 0u,
		.Flags                 = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	psoDesc.NumRenderTargets = 4u;
	for (UINT i = 0u; i < 4u; ++i) {
		psoDesc.BlendState.RenderTarget[i].BlendEnable           = false;
		psoDesc.BlendState.RenderTarget[i].SrcBlend              = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[i].DestBlend             = D3D12_BLEND_ZERO;
		psoDesc.BlendState.RenderTarget[i].BlendOp               = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha         = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[i].DestBlendAlpha        = D3D12_BLEND_ZERO;
		psoDesc.BlendState.RenderTarget[i].BlendOpAlpha          = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.RTVFormats[1] = DXGI_FORMAT_R16G16_FLOAT;
	psoDesc.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.RTVFormats[3] = DXGI_FORMAT_R8_UNORM;
	psoDesc.DSVFormat     = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);
	setD3DName(ret.Get(), "PBRDeferredSkinnedIndirectGBufferShader");
	return ret;
}

ComPtr<ID3D12PipelineState> createPBRDeferredSkinnedGBufferShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	auto vsCode = compileShader("pbrDeferredSkinned.hlsl", nullptr, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);
	auto psCode = compileShader("pbrDeferredSkinned.hlsl", nullptr, "PSMain", "ps_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

	auto elemDescs = std::vector<D3D12_INPUT_ELEMENT_DESC>{
		D3D12_INPUT_ELEMENT_DESC{ .SemanticName = "POSITION",     .SemanticIndex = 0u, .Format = DXGI_FORMAT_R32G32B32_FLOAT,       .InputSlot = 0u, .AlignedByteOffset = 0u, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0u },
		D3D12_INPUT_ELEMENT_DESC{ .SemanticName = "NORMAL",       .SemanticIndex = 0u, .Format = DXGI_FORMAT_R32G32B32_FLOAT,       .InputSlot = 1u, .AlignedByteOffset = 0u, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0u },
		D3D12_INPUT_ELEMENT_DESC{ .SemanticName = "TANGENT",      .SemanticIndex = 0u, .Format = DXGI_FORMAT_R32G32B32_FLOAT,       .InputSlot = 2u, .AlignedByteOffset = 0u, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0u },
		D3D12_INPUT_ELEMENT_DESC{ .SemanticName = "BITANGENT",    .SemanticIndex = 0u, .Format = DXGI_FORMAT_R32G32B32_FLOAT,       .InputSlot = 3u, .AlignedByteOffset = 0u, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0u },
		D3D12_INPUT_ELEMENT_DESC{ .SemanticName = "UV",           .SemanticIndex = 0u, .Format = DXGI_FORMAT_R32G32_FLOAT,           .InputSlot = 4u, .AlignedByteOffset = 0u, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0u },
		D3D12_INPUT_ELEMENT_DESC{ .SemanticName = "BONE_INDICES", .SemanticIndex = 0u, .Format = DXGI_FORMAT_R32G32B32A32_SINT,     .InputSlot = 5u, .AlignedByteOffset = 0u, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0u },
		D3D12_INPUT_ELEMENT_DESC{ .SemanticName = "BONE_WEIGHTS", .SemanticIndex = 0u, .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,    .InputSlot = 6u, .AlignedByteOffset = 0u, .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, .InstanceDataStepRate = 0u }
	};

	auto inputLayoutDesc = D3D12_INPUT_LAYOUT_DESC{
		.pInputElementDescs = elemDescs.data(),
		.NumElements        = static_cast<UINT>(elemDescs.size())
	};

	auto psoDesc = D3D12_GRAPHICS_PIPELINE_STATE_DESC{
		.pRootSignature = rootSig,
		.VS = vsCode.byteCode,
		.PS = psCode.byteCode,
		.BlendState = D3D12_BLEND_DESC{ .AlphaToCoverageEnable = false, .IndependentBlendEnable = false },
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
		.RasterizerState = D3D12_RASTERIZER_DESC{
			.FillMode              = D3D12_FILL_MODE_SOLID,
			.CullMode              = D3D12_CULL_MODE_BACK,
			.FrontCounterClockwise = false,
			.DepthBias             = 0,
			.DepthBiasClamp        = 0.f,
			.SlopeScaledDepthBias  = 0.f,
			.DepthClipEnable       = true,
			.MultisampleEnable     = false,
			.AntialiasedLineEnable = false,
			.ForcedSampleCount     = 0u
		},
		.DepthStencilState = D3D12_DEPTH_STENCIL_DESC{
			.DepthEnable      = true,
			.DepthWriteMask   = D3D12_DEPTH_WRITE_MASK_ALL,
			.DepthFunc        = D3D12_COMPARISON_FUNC_LESS,
			.StencilEnable    = false,
			.StencilReadMask  = 0u,
			.StencilWriteMask = 0u,
			.FrontFace        = D3D12_DEPTH_STENCILOP_DESC{},
			.BackFace         = D3D12_DEPTH_STENCILOP_DESC{}
		},
		.InputLayout           = inputLayoutDesc,
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.SampleDesc            = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
		.NodeMask              = 0u,
		.Flags                 = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	psoDesc.NumRenderTargets = 4u;
	for (UINT i = 0u; i < 4u; ++i) {
		psoDesc.BlendState.RenderTarget[i].BlendEnable           = false;
		psoDesc.BlendState.RenderTarget[i].SrcBlend              = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[i].DestBlend             = D3D12_BLEND_ZERO;
		psoDesc.BlendState.RenderTarget[i].BlendOp               = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha         = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[i].DestBlendAlpha        = D3D12_BLEND_ZERO;
		psoDesc.BlendState.RenderTarget[i].BlendOpAlpha          = D3D12_BLEND_OP_ADD;
		psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.RTVFormats[1] = DXGI_FORMAT_R16G16_FLOAT;
	psoDesc.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.RTVFormats[3] = DXGI_FORMAT_R8_UNORM;
	psoDesc.DSVFormat     = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);
	setD3DName(ret.Get(), "PBRDeferredSkinnedGBufferShader");
	return ret;
}

ComPtr<ID3D12PipelineState> createPBRDeferredLightingShader(ID3D12Device* device, ID3D12RootSignature* rootSig) {
	ComPtr<ID3D12PipelineState> ret{};

	auto vsCode = compileShader("pbrDeferredLighting.hlsl", nullptr, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);
	auto psCode = compileShader("pbrDeferredLighting.hlsl", nullptr, "PSMain", "ps_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u);

	// No vertex input — SV_VertexID fullscreen triangle
	auto inputLayoutDesc = D3D12_INPUT_LAYOUT_DESC{
		.pInputElementDescs = nullptr,
		.NumElements        = 0u
	};

	auto psoDesc = D3D12_GRAPHICS_PIPELINE_STATE_DESC{
		.pRootSignature = rootSig,
		.VS = vsCode.byteCode,
		.PS = psCode.byteCode,
		.BlendState = D3D12_BLEND_DESC{ .AlphaToCoverageEnable = false, .IndependentBlendEnable = false },
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
		.RasterizerState = D3D12_RASTERIZER_DESC{
			.FillMode              = D3D12_FILL_MODE_SOLID,
			.CullMode              = D3D12_CULL_MODE_NONE,   // fullscreen triangle: no culling
			.FrontCounterClockwise = false,
			.DepthBias             = 0,
			.DepthBiasClamp        = 0.f,
			.SlopeScaledDepthBias  = 0.f,
			.DepthClipEnable       = true,
			.MultisampleEnable     = false,
			.AntialiasedLineEnable = false,
			.ForcedSampleCount     = 0u
		},
		.DepthStencilState = D3D12_DEPTH_STENCIL_DESC{
			.DepthEnable      = false,                        // no depth test/write in lighting pass
			.DepthWriteMask   = D3D12_DEPTH_WRITE_MASK_ZERO,
			.DepthFunc        = D3D12_COMPARISON_FUNC_ALWAYS,
			.StencilEnable    = false,
			.StencilReadMask  = 0u,
			.StencilWriteMask = 0u,
			.FrontFace        = D3D12_DEPTH_STENCILOP_DESC{},
			.BackFace         = D3D12_DEPTH_STENCILOP_DESC{}
		},
		.InputLayout           = inputLayoutDesc,
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.SampleDesc            = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
		.NodeMask              = 0u,
		.Flags                 = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	// Output to single backbuffer RT
	psoDesc.NumRenderTargets = 1u;
	psoDesc.BlendState.RenderTarget[0].BlendEnable           = false;
	psoDesc.BlendState.RenderTarget[0].SrcBlend              = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[0].DestBlend             = D3D12_BLEND_ZERO;
	psoDesc.BlendState.RenderTarget[0].BlendOp               = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha         = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[0].DestBlendAlpha        = D3D12_BLEND_ZERO;
	psoDesc.BlendState.RenderTarget[0].BlendOpAlpha          = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	// No DSVFormat — depth test disabled

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), &ret),
		false
	);
	setD3DName(ret.Get(), "PBRDeferredLightingShader");
	return ret;
}

// Builds the PSO used by TrailPipeline. No vertex buffer is bound — the VS
// pulls each segment's endpoints from a StructuredBuffer (gTrailVertices) via
// SV_VertexID. CullMode = NONE so trails are visible from either side.
// `additive == true` uses additive blending (One/One); otherwise standard alpha.
static ComPtr<ID3D12PipelineState> createTrailShaderImpl(
	ID3D12Device* device, ID3D12RootSignature* rootSig, bool additive
) {
	ComPtr<ID3D12PipelineState> ret{};

	auto vsCode = compileShader( "trail.hlsl", nullptr, "VSMain", "vs_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u );
	auto psCode = compileShader( "trail.hlsl", nullptr, "PSMain", "ps_6_0", D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES, 0u );

	// No input layout — VS reads from StructuredBuffer indexed by SV_VertexID.
	auto inputLayoutDesc = D3D12_INPUT_LAYOUT_DESC{
		.pInputElementDescs = nullptr,
		.NumElements = 0u
	};

	auto psoDesc = D3D12_GRAPHICS_PIPELINE_STATE_DESC{
		.pRootSignature = rootSig,
		.VS = vsCode.byteCode,
		.PS = psCode.byteCode,
		.BlendState = D3D12_BLEND_DESC{
			.AlphaToCoverageEnable = false,
			.IndependentBlendEnable = false
		},
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
		.RasterizerState = D3D12_RASTERIZER_DESC{
			.FillMode = D3D12_FILL_MODE_SOLID,
			.CullMode = D3D12_CULL_MODE_NONE,
			.FrontCounterClockwise = false,
			.DepthBias = 0,
			.DepthBiasClamp = 0.f,
			.SlopeScaledDepthBias = 0.f,
			.DepthClipEnable = true,
			.MultisampleEnable = false,
			.AntialiasedLineEnable = false,
			.ForcedSampleCount = 0u
		},
		// Depth-test against scene geometry but do NOT write depth — overlapping
		// trail segments must all blend correctly.
		.DepthStencilState = D3D12_DEPTH_STENCIL_DESC{
			.DepthEnable = true,
			.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
			.DepthFunc = D3D12_COMPARISON_FUNC_LESS,
			.StencilEnable = false,
			.StencilReadMask = 0u,
			.StencilWriteMask = 0u,
			.FrontFace = D3D12_DEPTH_STENCILOP_DESC{},
			.BackFace = D3D12_DEPTH_STENCILOP_DESC{}
		},
		.InputLayout = inputLayoutDesc,
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.SampleDesc = DXGI_SAMPLE_DESC{ .Count = 1u, .Quality = 0u },
		.NodeMask = 0u,
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE
	};

	psoDesc.NumRenderTargets = 1u;
	psoDesc.BlendState.RenderTarget[0].BlendEnable = true;
	if (additive) {
		// Additive: src * 1 + dst * 1
		psoDesc.BlendState.RenderTarget[0].SrcBlend  = D3D12_BLEND_ONE;
		psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	} else {
		// Standard alpha blend
		psoDesc.BlendState.RenderTarget[0].SrcBlend  = D3D12_BLEND_SRC_ALPHA;
		psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	}
	psoDesc.BlendState.RenderTarget[0].BlendOp        = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha  = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	psoDesc.BlendState.RenderTarget[0].BlendOpAlpha   = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat     = DXGI_FORMAT_D32_FLOAT;

	DISPLAY_ERROR_DX_HR(
		device->CreateGraphicsPipelineState( &psoDesc, __uuidof(ID3D12PipelineState), &ret ),
		false
	);

	setD3DName( ret.Get(), additive ? "TrailShaderAdditive" : "TrailShader" );

	return ret;
}

ComPtr<ID3D12PipelineState> createTrailShader( ID3D12Device* device, ID3D12RootSignature* rootSig ) {
	return createTrailShaderImpl( device, rootSig, /*additive=*/false );
}

ComPtr<ID3D12PipelineState> createTrailShaderAdditive( ID3D12Device* device, ID3D12RootSignature* rootSig ) {
	return createTrailShaderImpl( device, rootSig, /*additive=*/true );
}
