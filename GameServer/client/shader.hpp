#ifndef __shader_HPP
#define __shader_HPP

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
ComPtr<ID3D12PipelineState> createShadowMapCSMShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createShadowMapSkinnedShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createShadowMapSkinnedCSMShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createPBRShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createPBRShaderCSMDebug(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createPBRDeferredGBufferShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createPBRDeferredSkinnedGBufferShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createPBRDeferredLightingShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createPBRSkinnedShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createPBRSkinnedShaderCSMDebug(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createBillboardShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createBillboardShaderAdditive(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createMeshParticleShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createSkyboxShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createBVShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createUIShader( ID3D12Device* device, ID3D12RootSignature* rootSig );
ComPtr<ID3D12PipelineState> createTerrainShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createTerrainShaderCSMDebug(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createTerrainShadowMapShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createTerrainShadowMapCSMShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createTerrainDeferredGBufferShader(ID3D12Device* device, ID3D12RootSignature* rootSig);

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
	u32t firstInstanceOffset;
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
	XMFLOAT3X3 worldNormal;  // inverse(Mat3x3(world)) — non-uniform scale safe normal transform
};

struct PerDrawcallData {
	Material material;
	u32t firstInstanceOffset;
};

struct PerFrameData {
	XMFLOAT3   globalAmbient;
	float      padding0;
	u32t       lightCnt;
	u32t       cascadeCount;
	XMUINT2    padding1;
	BindlessIndex idxShadowMap[MAX_CSM_CASCADES];  // cascade별 독립 Texture2D SRV
	XMFLOAT4   cascadeSplitsFarV;
	XMFLOAT4X4 lightVP[MAX_CSM_CASCADES];
	XMFLOAT4   cascadeNormalOffsets;  // world units of normal offset per cascade (for shadow acne elimination)
};
}	// namespace PBRShader

// PBRSkinnedShader
namespace PBRSkinnedShader {
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

struct BoneData {
	XMFLOAT4X4 xform;
};

struct PerInstanceData {
	XMFLOAT4X4 world;
	XMFLOAT4X4 wvp;
	XMFLOAT4X4 wv;
	XMFLOAT3X3 wvNormal;
	XMFLOAT3X3 worldNormal;  // inverse(Mat3x3(world)) — non-uniform scale safe normal transform
	u32t rootBoneOffset;
	XMUINT3 padding;
};

struct PerDrawcallData {
	Material material;
	u32t firstInstanceOffset;
};

struct PerFrameData {
	XMFLOAT3   globalAmbient;
	float      padding0;
	u32t       lightCnt;
	u32t       cascadeCount;
	XMUINT2    padding1;
	BindlessIndex idxShadowMap[MAX_CSM_CASCADES];  // cascade별 독립 Texture2D SRV
	XMFLOAT4   cascadeSplitsFarV;
	XMFLOAT4X4 lightVP[MAX_CSM_CASCADES];
	XMFLOAT4   cascadeNormalOffsets;
};
}	// namespace PBRSkinnedShader

// BillboardShader
namespace BillboardShader {

struct Material {
	BindlessIndex idxTex;
	XMFLOAT4 tint;	// RGBA — rgb: 색 multiplier, a: 알파 multiplier. finalColor = startColor * tint (component-wise)
};

struct PerInstanceData {
	XMFLOAT4X4 world;
	float       rotation;   // 빌보드 평면 내 회전 (라디안)
	float       pad[3];
};

// cbuffer b0 레이아웃 (총 56B):
//   Material(32B) | firstInstanceOffset(4B) | uvOffset(8B) | pad_(4B) | uvScale(8B)
// pad_는 HLSL cbuffer packing rule에 의해 float2 uvScale이 16-byte register 경계(48B)로
// 자동 정렬되는 것에 맞추기 위한 명시적 패딩이다.
struct PerDrawcallData {
	Material material;
	u32t firstInstanceOffset;
	XMFLOAT2 uvOffset;		// 스프라이트 시트 내 현재 프레임의 좌상단 UV
	u32t pad_;				// 명시적 4B 패딩 — HLSL register boundary 정렬 (offset 44→48)
	XMFLOAT2 uvScale;		// 현재 프레임의 UV 크기 (= 1/cols, 1/rows)
};

struct PerFrameData {
	XMFLOAT4X4 vp;
	XMFLOAT3 cameraPosW;  // world-space camera position
	float padding0;
};

}	// namespace BillboardShader

// MeshParticleShader
namespace MeshParticleShader {

// 80B, 16B-aligned
// world: row-major — CPU에서 mu::transpose().getXmf() 후 전달
struct PerInstanceData {
	XMFLOAT4X4 world;  // 64B
	XMFLOAT4   tint;   // 16B
};

// 32B
// int4 idxTex(16B) | firstInstanceOffset(4B) | pad(12B)
struct PerDrawcallData {
	BindlessIndex idxTex;              // int4, 16B
	u32t          firstInstanceOffset; // 4B
	XMUINT3       pad;                 // 12B
};

// 64B
struct PerFrameData {
	XMFLOAT4X4 vp;  // row-major — CPU에서 mu::transpose().getXmf() 후 전달
};

}	// namespace MeshParticleShader

// ShadowMapShader
namespace ShadowMapShader {
struct PerInstanceData {
	XMFLOAT4X4 world;
};

struct PerDrawcallData {
	u32t firstInstanceOffset;
	XMUINT3 padding;
};

struct PerFrameData {
	XMFLOAT4X4 lightVP;
};
}	// namespace ShadowMapShader

// ShadowMapCSMShader — Matches shadowMapCSM.hlsl (separate Texture2D per cascade, no GS)
namespace ShadowMapCSMShader {
struct PerInstanceData {
	XMFLOAT4X4 world;
};

struct PerDrawcallData {
	u32t firstInstanceOffset;
	XMUINT3 padding;
};

struct PerFrameData {
	XMFLOAT4X4 lightVP;    // 현재 cascade의 VP 행렬 (cascade pass마다 갱신)
	u32t       cascadeIdx;
	XMUINT3    _pfd0;
};
}	// namespace ShadowMapCSMShader

// ShadowMapSkinnedShader
namespace ShadowMapSkinnedShader {
struct BoneData {
	XMFLOAT4X4 xform;
};

struct PerInstanceData {
	XMFLOAT4X4 world;
	u32t rootBoneOffset;
	XMUINT3 padding;
};

struct PerDrawcallData {
	u32t firstInstanceOffset;
	XMUINT3 padding;
};

struct PerFrameData {
	XMFLOAT4X4 lightVP;
};
}	// namespace ShadowMapSkinnedShader

// ShadowMapSkinnedCSMShader — Matches shadowMapSkinnedCSM.hlsl (separate Texture2D per cascade, no GS)
namespace ShadowMapSkinnedCSMShader {
struct BoneData {
	XMFLOAT4X4 xform;
};

struct PerInstanceData {
	XMFLOAT4X4 world;
	u32t rootBoneOffset;
	XMUINT3 padding;
};

struct PerDrawcallData {
	u32t firstInstanceOffset;
	XMUINT3 padding;
};

struct PerFrameData {
	XMFLOAT4X4 lightVP;    // 현재 cascade의 VP 행렬 (cascade pass마다 갱신)
	u32t       cascadeIdx;
	XMUINT3    _pfd0;
};
}	// namespace ShadowMapSkinnedCSMShader

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
	u32t firstInstanceOffset;
	XMUINT3 padding;
};
}	// namespace BoundingVolumeShader

// TerrainShader
namespace TerrainShader {

constexpr int MAX_TERRAIN_LAYERS = 4;

// Matches cbuffer PerDrawcallData : register(b0) in terrain.hlsl
struct PerDrawcallData {
    XMFLOAT4X4 wvp;
    XMFLOAT4X4 world;
    XMFLOAT4X4 wv;           // world-view (replaces worldNormal; uniform scale assumed)

    BindlessIndex idxSplatMap;
    BindlessIndex idxDiffuse[MAX_TERRAIN_LAYERS];
    BindlessIndex idxNormal [MAX_TERRAIN_LAYERS];
    XMFLOAT4     tiling            [MAX_TERRAIN_LAYERS];  // (tileSizeX, tileSizeY, tileOffsetX, tileOffsetY)
    XMFLOAT4     metallicRoughness[MAX_TERRAIN_LAYERS];  // x=metallic, y=roughness, zw=unused
    int          layerCount;
    int          hasAnyNormal;
    float        _pdd0[2];
};

// Matches cbuffer PerFrameData : register(b1) in terrain.hlsl
// Same layout as PBRShader::PerFrameData
struct PerFrameData {
    XMFLOAT3   globalAmbient;
    float      padding0;
    u32t       lightCnt;
    u32t       cascadeCount;
    XMUINT2    padding1;
    BindlessIndex idxShadowMap[MAX_CSM_CASCADES];  // cascade별 독립 Texture2D SRV
    XMFLOAT4   cascadeSplitsFarV;
    XMFLOAT4X4 lightVP[MAX_CSM_CASCADES];
    XMFLOAT4   cascadeNormalOffsets;
};

}	// namespace TerrainShader

// TerrainShadowMapShader
namespace TerrainShadowMapShader {
// Matches cbuffer PerDrawcallData : register(b0) in terrainShadowMap.hlsl (non-CSM)
struct PerDrawcallData {
    XMFLOAT4X4 world;
};
// Matches cbuffer PerFrameData : register(b1) in terrainShadowMap.hlsl (non-CSM)
struct PerFrameData {
    XMFLOAT4X4 lightVP;
};
}  // namespace TerrainShadowMapShader

// TerrainShadowMapCSMShader — Matches terrainShadowMapCSM.hlsl (separate Texture2D per cascade, no GS)
namespace TerrainShadowMapCSMShader {
struct PerDrawcallData {
    XMFLOAT4X4 world;
};
struct PerFrameData {
    XMFLOAT4X4 lightVP;    // 현재 cascade의 VP 행렬 (cascade pass마다 갱신)
    u32t       cascadeIdx;
    XMUINT3    _pfd0;
};
}  // namespace TerrainShadowMapCSMShader

// TerrainDeferredGBufferShader — Matches terrainDeferred.hlsl (GBuffer geometry pass for terrain)
namespace TerrainDeferredGBufferShader {
// PerDrawcallData is identical to TerrainShader::PerDrawcallData — reuse that struct.
// Matches cbuffer PerFrameData : register(b1) in terrainDeferred.hlsl
struct PerFrameData {
	XMFLOAT3 globalAmbient;
	float    _pad;
};
}  // namespace TerrainDeferredGBufferShader

// PBRDeferredGBufferShader — same data layout as PBRShader (static mesh GBuffer geometry pass)
namespace PBRDeferredGBufferShader {
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
	XMFLOAT3X3 worldNormal;  // inverse(Mat3x3(world)) — non-uniform scale safe normal transform
};

struct PerDrawcallData {
	Material material;
	u32t firstInstanceOffset;
};

struct PerFrameData {
	XMFLOAT3   globalAmbient;
	float      padding0;
	u32t       lightCnt;
	u32t       cascadeCount;
	XMUINT2    padding1;
	BindlessIndex idxShadowMap[MAX_CSM_CASCADES];  // cascade별 독립 Texture2D SRV
	XMFLOAT4   cascadeSplitsFarV;
	XMFLOAT4X4 lightVP[MAX_CSM_CASCADES];
	XMFLOAT4   cascadeNormalOffsets;  // world units of normal offset per cascade (for shadow acne elimination)
};
}	// namespace PBRDeferredGBufferShader

// PBRDeferredSkinnedGBufferShader — same data layout as PBRSkinnedShader (skinned mesh GBuffer geometry pass)
namespace PBRDeferredSkinnedGBufferShader {
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

struct BoneData {
	XMFLOAT4X4 xform;
};

struct PerInstanceData {
	XMFLOAT4X4 world;
	XMFLOAT4X4 wvp;
	XMFLOAT4X4 wv;
	XMFLOAT3X3 wvNormal;
	XMFLOAT3X3 worldNormal;  // inverse(Mat3x3(world)) — non-uniform scale safe normal transform
	u32t rootBoneOffset;
	XMUINT3 padding;
};

struct PerDrawcallData {
	Material material;
	u32t firstInstanceOffset;
};

struct PerFrameData {
	XMFLOAT3   globalAmbient;
	float      padding0;
	u32t       lightCnt;
	u32t       cascadeCount;
	XMUINT2    padding1;
	BindlessIndex idxShadowMap[MAX_CSM_CASCADES];  // cascade별 독립 Texture2D SRV
	XMFLOAT4   cascadeSplitsFarV;
	XMFLOAT4X4 lightVP[MAX_CSM_CASCADES];
	XMFLOAT4   cascadeNormalOffsets;
};
}	// namespace PBRDeferredSkinnedGBufferShader

// PBRDeferredLightingShader — fullscreen triangle deferred lighting pass
namespace PBRDeferredLightingShader {

// Matches cbuffer PerFrameData : register(b0) in pbrDeferredLighting.hlsl
struct PerFrameData {
	// CSM / lighting (same as PBRShader::PerFrameData)
	XMFLOAT3   globalAmbient;
	float      _pfd0;
	u32t       lightCnt;
	u32t       cascadeCount;
	XMUINT2    _pfd1;
	BindlessIndex idxShadowMap[MAX_CSM_CASCADES];
	XMFLOAT4   cascadeSplitsFarV;
	XMFLOAT4X4 lightVP[MAX_CSM_CASCADES];
	XMFLOAT4   cascadeNormalOffsets;
	// Deferred reconstruction
	XMFLOAT4X4 invView;
	XMFLOAT4X4 invProj;
	// GBuffer bindless SRV indices
	BindlessIndex idxGB0;
	BindlessIndex idxGB1;
	BindlessIndex idxGB2;
	BindlessIndex idxGB3;
	BindlessIndex idxDepth;
	// Debug
	u32t       debugMode;
	XMUINT3    _pad;
};

}	// namespace PBRDeferredLightingShader

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