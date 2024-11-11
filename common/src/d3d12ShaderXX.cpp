#include "d3d12ShaderXX.hpp"

#include <d3dcompiler.h>

namespace gfx {

namespace d3d12 {

ShaderBlob::ShaderBlob( const std::filesystem::path& path,
	const InputLayout& inputLayout, const D3D_SHADER_MACRO* macros,
	std::string_view entryPoint, std::string_view target,
	UINT flag1, UINT flag2, Type type
) : D3DWrapper<ID3DBlob>(), type_(type) {
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
) : D3DWrapper<InterfaceType>(), pShader_(&shader) {
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

}   // namespace gfx::d3d12

}   // namespace gfx