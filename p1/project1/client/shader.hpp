#ifndef __shader_HPP
#define __shader_HPP

#include "pch.hpp"

struct CompiledShaderOutput {
	ComPtr<ID3DBlob> blob;
	D3D12_SHADER_BYTECODE byteCode;
};

// 셰이더를 컴파일하여 D3D Blob 객체, 그리고 그 객체와 연결된
// D3D12_SHADER_BYTECODE 객체를 리턴한다.
CompiledShaderOutput compileShader(const std::filesystem::path& path,
	const D3D_SHADER_MACRO* macros,
	std::string_view entryPoint, std::string_view target,
	UINT flag1, UINT flag2
);

ComPtr<ID3D12PipelineState> createSampleShader(ID3D12Device* device, ID3D12RootSignature* rootSig);

// 루트 파라미터 접근을 이해하기 쉽도록 하기 위해 만든 클래스
// 루트 파라미터에 이름을 지어 그 인덱스 및 D3D12_ROOT_PARAMETER 구조체와 매핑한다.
class RootSig {
public:
	virtual void build(ID3D12Device* device) = 0;
	virtual const std::wstring& name() const = 0;

	UINT paramIdx(std::string_view paramName) const;
	const D3D12_ROOT_PARAMETER& paramDesc(std::string_view paramName) const;

	ID3D12RootSignature* get() const { return rootSig_.Get(); }

protected:
	void addParam(std::string paramName, UINT paramIdx, const D3D12_ROOT_PARAMETER& paramDesc);

	ComPtr<ID3D12RootSignature> rootSig_{};
	// key: 루트 파라미터 이름, value: 루트 파라미터 인덱스와 루트 파라미터 구조체의 pair
	std::map<std::string, std::pair<UINT, D3D12_ROOT_PARAMETER>> paramMap_{};
	std::wstring name_{L"unbuilt root signature"};
};

class DefaultRootSig : public RootSig {
public:
	void build(ID3D12Device* device) override;
	const std::wstring& name() const override;

private:
};

#endif	// __shader_HPP