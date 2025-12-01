#ifndef __shader_HPP
#define __shader_HPP

#include "pch.hpp"
#include "gfxUtil.hpp"

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
ComPtr<ID3D12PipelineState> createShadowMapShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createPBRShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createBillboardShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createSkyboxShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createBVShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createUIShader( ID3D12Device* device, ID3D12RootSignature* rootSig );

// 루트 파라미터 접근을 이해하기 쉽도록 하기 위해 만든 클래스
// 루트 파라미터에 이름을 지어 그 인덱스 및 D3D12_ROOT_PARAMETER 구조체와 매핑한다.
class RootSig {
public:
	virtual ~RootSig() = default;

	virtual void build(ID3D12Device* device) = 0;
	virtual const std::string& name() const = 0;

	UINT paramIdx(std::string_view paramName) const;
	const D3D12_ROOT_PARAMETER& paramDesc(std::string_view paramName) const;

	ID3D12RootSignature* get() const { return rootSig_.Get(); }

protected:
	void addParam(const std::string& paramName, UINT paramIdx, const D3D12_ROOT_PARAMETER& paramDesc);

	ComPtr<ID3D12RootSignature> rootSig_ = nullptr;
	// key: 루트 파라미터 이름, value: 루트 파라미터 인덱스와 루트 파라미터 구조체의 pair
	std::map<std::string, std::pair<UINT, D3D12_ROOT_PARAMETER>> paramMap_{};
	std::string name_{"unbuilt root signature"};
};

class DefaultRootSig : public RootSig {
public:
	void build(ID3D12Device* device) override;
	const std::string& name() const override;

private:
};

// 셰이더별 구조체 ------------------------------------
// SampleShader
namespace SampleShader {

struct Material {
	BindlessIndex idxAlbedo;
	BindlessIndex idxRoughness;
	BindlessIndex idxMetallic;

	XMFLOAT4 cAlbedo;
	float cRoughness;
	float cMetallic;
};

struct PerInstanceData {
	XMFLOAT4X4 wvp;
};

struct PerDrawcallData {
	Material material;
	u32t firstInstanceIdx;
};

}	// namespace SampleShader

// PBRShader
namespace PBRShader {
struct Light {
	enum class Type {
		PointLight,
		Spotlight,
		DirectionalLight
	};

	XMFLOAT3 color;
	float falloff;
	XMFLOAT3 posV;
	float cosTheta;
	XMFLOAT3 dirV;
	float cosPhi;
	XMFLOAT3 atten;
	float intensity;
	int type;
	XMINT3 padding;
};

struct Material {
	BindlessIndex idxAlbedo;
	BindlessIndex idxMetallicSmoothness;	// 유니티 익스포터를 사용하기 때문에 유니티와 텍스처 포맷 맞춰준다.
											// R 채널에 metallic, A 채널에 Smoothness (1 - roughness) 값이 들어있게 된다.
	BindlessIndex idxNormal;
	BindlessIndex idxEmmisive;
	BindlessIndex idxAmbientOcllusion;

	XMFLOAT4 cAlbedo;
	float cRoughness;
	float cMetallic;
	float cAOStrength;
	float padding0;
	XMFLOAT3 cEmmisive;
	float padding1;
};

struct PerInstanceData {
	XMFLOAT4X4 world;
	XMFLOAT4X4 wvp;
	XMFLOAT4X4 wv;
	XMFLOAT3X3 wvNormal;
};

struct PerDrawcallData {
	Material material;
	u32t firstInstanceIdx;
};

struct PerFrameData {
	XMFLOAT3 globalAmbient;
	float padding0;
	u32t lightCnt;
	XMUINT3 padding1;
	BindlessIndex idxShadowMap;
	XMFLOAT4X4 lightVP;
};
}	// namespace PBRShader

// BillboardShader
namespace BillboardShader {

struct Material {
	BindlessIndex idxAlbedo;
	BindlessIndex idxRoughness;
	BindlessIndex idxMetallic;

	XMFLOAT4 cAlbedo;
	float cRoughness;
	float cMetallic;
};

struct PerInstanceData {
	XMFLOAT4X4 world;
};

struct PerDrawcallData {
	Material material;
	u32t firstInstanceIdx;
};

struct PerFrameData {
	XMFLOAT4X4 vp;
	XMFLOAT3 cameraPosV;
	float padding0;
};

}	// namespace BillboardShader

// ShadowMapShader
namespace ShadowMapShader {
struct PerInstanceData {
	XMFLOAT4X4 world;
};

struct PerDrawcallData {
	u32t firstInstanceIdx;
	XMUINT3 padding;
};

struct PerFrameData {
	XMFLOAT4X4 lightVP;
};
}	// namespace ShadowMapShader

// SkyboxShader
namespace SkyboxShader {
struct Material {
	BindlessIndex idxAlbedo;
};

struct PerDrawcallData {
	Material material;
};

struct PerFrameData {
	XMFLOAT4X4 viewProj;
};
}	// namespace SkyboxShader

// BoundingVolumeShader
namespace BVShader {
struct PerInstanceData {
	XMFLOAT4X4 wvp;
	XMFLOAT4 color;
};

struct PerDrawcallData {
	u32t firstInstanceIdx;
	XMUINT3 padding;
};
}	// namespace BoundingVolumeShader

namespace UIShader {
	struct Material {
		BindlessIndex idxAlbedo;
		BindlessIndex idxRoughness;
		BindlessIndex idxMetallic;

		XMFLOAT4 cAlbedo;
		float cRoughness;
		float cMetallic;
	};

	struct PerInstanceData {
		XMFLOAT4X4 world;
	};

	struct PerDrawcallData {
		Material material;
		u32t firstInstanceIdx;
	};

	struct PerFrameData {
		float screenWidth;
		float screenHeight;
		XMFLOAT2 padding;
	};

} // namespace UIShader

#endif	// __shader_HPP