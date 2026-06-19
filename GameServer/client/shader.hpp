#ifndef __shader_HPP
#define __shader_HPP

#include "gfxUtil.hpp"

struct CompiledShaderOutput {
	ComPtr<IDxcBlob> blob;
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
// Alpha-tested (masked) static-mesh CSM shadow PSO for foliage (Position+UV, CULL_NONE, clip PS).
ComPtr<ID3D12PipelineState> createShadowMapCSMMaskedShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createShadowMapSkinnedShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createShadowMapSkinnedCSMShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createPBRShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createPBRShaderCSMDebug(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createPBRDeferredGBufferShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createPBRDeferredIndirectGBufferShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createPBRDeferredSkinnedIndirectGBufferShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createPBRDeferredSkinnedGBufferShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createPBRDeferredLightingShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createTonemapResolveShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createBloomPrefilterShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createBloomDownsampleShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createBloomUpsampleShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createPBRSkinnedShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createPBRSkinnedShaderCSMDebug(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createBillboardShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createBillboardShaderAdditive(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createBillboardShaderMultiply(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createBillboardShaderPremultiplied(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createMeshParticleShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createEnergyOrbShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createWindRingShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createSmokeBlendCGShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createBlendCGMeshShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createPiercingMeshShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createPiercingSlashMeshShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createSwordSlashShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createTwoSidesShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createTrailShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createTrailShaderAdditive(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createSkyboxShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createBVShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createUIShader( ID3D12Device* device, ID3D12RootSignature* rootSig );
ComPtr<ID3D12PipelineState> createTerrainShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createTerrainShaderCSMDebug(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createTerrainShadowMapShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createTerrainShadowMapCSMShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createTerrainDeferredGBufferShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createHiZOccluderShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createHiZMapShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createHiZClearShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createHiZCullShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createHiZCompactShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createHiZCommandShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createPrefixSumShader(ID3D12Device* device, ID3D12RootSignature* rootSig);

// IBL runtime precompute compute shaders (irradiance / specular prefilter / BRDF LUT).
// All three share the IBLShader::IBLParams cbuffer bound at "PerDrawcallData" (b0)
// and write to a UAV bound at "DestTex" (u0). The source environment cube is read
// bindlessly (TextureCubePool + SamplerPool).
ComPtr<ID3D12PipelineState> createIBLIrradianceShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createIBLPrefilterShader(ID3D12Device* device, ID3D12RootSignature* rootSig);
ComPtr<ID3D12PipelineState> createIBLBRDFLUTShader(ID3D12Device* device, ID3D12RootSignature* rootSig);

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

class CmdSig {
public:
	void build(ID3D12Device* device, const RootSig& root);
	ID3D12CommandSignature* get() const { return cmdSig_.Get(); }

private:
	ComPtr<ID3D12CommandSignature> cmdSig_;
	std::string name_{"unbuilt command signature"};
};

struct DrawIndexedInstancedArgs
{
    u32t IndexCountPerInstance;
    u32t InstanceCount;
    u32t StartIndexLocation;
    i32t  BaseVertexLocation;
    u32t StartInstanceLocation;
};

// 셰이더별 구조체 ------------------------------------
// IBLShader — shared cbuffer for the IBL precompute compute shaders.
// Layout MUST match the "IBLParams" cbuffer (register b0) in iblIrradiance.hlsl /
// iblPrefilter.hlsl / iblBRDFLUT.hlsl. 48 bytes total.
//   idxEnv    : BindlessIndex (HLSL int4) — source environment cube SRV
//   faceRes   : output face / LUT resolution in pixels
//   mipLevel  : current mip index (prefilter only)
//   mipCount  : total mip count   (prefilter only)
//   envIsLDR  : 1 => recover approx HDR from an LDR source
//   roughness : current mip roughness in [0,1] (prefilter only)
//   _pad      : padding to 48 bytes
namespace IBLShader {

struct IBLParams {
	BindlessIndex idxEnv;
	u32t          faceRes;
	u32t          mipLevel;
	u32t          mipCount;
	u32t          envIsLDR;
	float         roughness;
	XMFLOAT3      _pad;
};
static_assert(sizeof(IBLParams) == 48u, "IBLShader::IBLParams must match the 48-byte HLSL IBLParams cbuffer.");

}	// namespace IBLShader

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
	float cAlphaCutoff;   // foliage alpha-test threshold (0 = opaque, no discard)
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
	// IBL (forward parity) — must match cbuffer PerFrameData in pbr.hlsl
	XMFLOAT3      camPos;
	float         _padCam;
	BindlessIndex idxIrradiance;
	BindlessIndex idxPrefiltered;
	BindlessIndex idxBRDFLUT;
	u32t          prefilteredMipCount;
	float         iblIntensity;
	XMFLOAT2      _iblPad;
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
	// IBL (forward parity) — must match cbuffer PerFrameData in pbrSkinned.hlsl
	XMFLOAT3      camPos;
	float         _padCam;
	BindlessIndex idxIrradiance;
	BindlessIndex idxPrefiltered;
	BindlessIndex idxBRDFLUT;
	u32t          prefilteredMipCount;
	float         iblIntensity;
	XMFLOAT2      _iblPad;
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
	XMFLOAT4X4 rotation3D; // transposed before upload; local billboard 3D start/lifetime rotation
	XMFLOAT4   stretchAxisAndMode; // xyz=world axis, w=1 when stretched
	float      rotation;   // 빌보드 평면 내 회전 (라디안)
	float      alignmentMode; // 0=view-facing, 1=local/world axes
	float      pad[2];
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

// EnergyOrbShader
namespace EnergyOrbShader {

// 112B, 16B-aligned
// world: row-major — CPU transposes (mu::transpose().getXmf()) before upload
struct PerInstanceData {
	XMFLOAT4X4 world;              // 64B  death-time object world
	XMFLOAT4   colorAndSize;       // 16B  rgb = HDR emissive color, a = point size
	XMFLOAT4   sphereCenterRadius; // 16B  xyz = orb center (world), w = sphere radius
	u32t       rootBoneOffset;     // 4B   index into the bone palette
	float      morphT;             // 4B   0 = mesh pose, 1 = collapsed sphere
	u32t       vertexCount;        // 4B   submesh vertex count (hash normalization)
	float      pad;                // 4B
};

// 32B
struct PerDrawcallData {
	BindlessIndex idxAlbedo;           // 16B  submesh albedo (idxRange < 0 = none)
	u32t          firstInstanceOffset; // 4B
	XMUINT3       pad0;                // 12B
};

// 80B, 16B-aligned
struct PerFrameData {
	XMFLOAT4X4 vp;         // 64B  row-major
	XMFLOAT3   cameraPosW; // 12B
	float      padding0;   // 4B
};

// 64B — one skinning matrix per bone (death-pose snapshot), row-major.
struct BoneData {
	XMFLOAT4X4 transform;
};

}	// namespace EnergyOrbShader

// WindRingShader
namespace WindRingShader {

// 80B, 16B-aligned
struct PerInstanceData {
	XMFLOAT4X4 world;  // 64B  row-major
	XMFLOAT4   tint;   // 16B
};

// 48B, 16B-aligned
struct PerDrawcallData {
	BindlessIndex idxTex;               // 16B
	u32t          firstInstanceOffset;  // 4B
	float         edgeFadePower;        // 4B
	float         edgeFadeStrength;     // 4B
	float         pad0;                 // 4B
	XMFLOAT3      cameraPosW;           // 12B
	float         pad1;                 // 4B
};

// 64B
struct PerFrameData {
	XMFLOAT4X4 vp;  // row-major
};

}	// namespace WindRingShader

// SmokeBlendCGShader
namespace SmokeBlendCGShader {

struct PerInstanceData {        // 176B
	XMFLOAT4X4 world;           // 64B
	XMFLOAT4X4 rotation3D;      // 64B
	XMFLOAT4   tint;            // 16B
	XMFLOAT4   stretchAxisAndMode;// 16B, xyz=world axis, w=1 when stretched
	float      rotation;        // 4B
	XMFLOAT3   pad;             // 12B
};

struct PerDrawcallData {        // 288B
	BindlessIndex idxMainTex;       // 16B
	BindlessIndex idxNoiseTex;      // 16B
	BindlessIndex idxFlowTex;       // 16B
	BindlessIndex idxMaskTex;       // 16B
	BindlessIndex idxCameraDepthTex;// 16B

	u32t firstInstanceOffset;       // 4B
	u32t hasNoiseTex;               // 4B
	u32t hasFlowTex;                // 4B
	u32t hasMaskTex;                // 4B

	u32t  hasCameraDepthTex;        // 4B
	float time;                     // 4B
	float cameraNear;               // 4B
	float cameraFar;                // 4B

	XMFLOAT4 mainTexST;             // 16B
	XMFLOAT4 noiseTexST;            // 16B
	XMFLOAT4 flowTexST;             // 16B
	XMFLOAT4 maskTexST;             // 16B

	XMFLOAT4 speedMainTexUVNoiseZW; // 16B
	XMFLOAT4 distortionSpeedXYPowerZ;// 16B
	XMFLOAT4 color;                 // 16B
	XMFLOAT4 uvRect;                // 16B, xy=offset, zw=scale

	float emission;                 // 4B
	float opacity;                  // 4B
	float textureOpacity;           // 4B
	float multiplyTexture;          // 4B

	float useOnlyColor;             // 4B
	float useFresnel;               // 4B
	float fresnelPower;             // 4B
	float fresnelScale;             // 4B

	float useCenterGlow;            // 4B
	float useDepth;                 // 4B
	float depthPower;               // 4B
	float pad0;                     // 4B
};

struct PerFrameData {           // 80B
	XMFLOAT4X4 matViewProj;     // 64B
	XMFLOAT3   cameraPosW;      // 12B
	float      pad1;            // 4B
};

}  // namespace SmokeBlendCGShader

// BlendCGMeshShader
namespace BlendCGMeshShader {

struct PerInstanceData {        // 112B
	XMFLOAT4X4 world;           // 64B
	XMFLOAT4   tint;            // 16B
	XMFLOAT2   custom1;         // 8B
	XMFLOAT2   custom2;         // 8B
	float      t;               // 4B
	float      customDataEnabled;// 4B
	XMFLOAT2   pad;             // 8B
};

using PerDrawcallData = SmokeBlendCGShader::PerDrawcallData;
using PerFrameData = SmokeBlendCGShader::PerFrameData;

}  // namespace BlendCGMeshShader

// PiercingMeshShader
namespace PiercingMeshShader {

using PerInstanceData = BlendCGMeshShader::PerInstanceData;  // 112B
using PerFrameData    = SmokeBlendCGShader::PerFrameData;    // 80B

struct PerDrawcallData {
	BindlessIndex idxColorNoiseTex;        // 16B
	BindlessIndex idxPiercingTex;          // 16B
	BindlessIndex idxPiercingNoiseTex;     // 16B
	BindlessIndex idxDistortionNoiseTex;   // 16B
	BindlessIndex idxDistortionMaskTex;    // 16B
	BindlessIndex idxEmissiveNoiseTex;     // 16B
	BindlessIndex idxEmissiveMaskTex;      // 16B
	BindlessIndex idxOpacityMaskTex;       // 16B

	u32t firstInstanceOffset;              // 4B
	u32t hasDistortionMask;                // 4B
	u32t hasEmissiveMask;                  // 4B
	u32t hasOpacityMask;                   // 4B

	float time;                            // 4B
	float colorBoost;                      // 4B
	float piercingNoiseIntensity;          // 4B
	float distortionIntensity;             // 4B

	float    emissiveIntensity;            // 4B
	float    opacityBoost;                 // 4B
	XMFLOAT2 pad0;                         // 8B

	XMFLOAT4 color1;                       // 16B
	XMFLOAT4 color2;                       // 16B
	XMFLOAT4 emissiveColor;                // 16B

	XMFLOAT4 colorNoiseScaleSpeed;         // 16B, xy=scale zw=speed
	XMFLOAT4 piercingNoiseScaleSpeed;      // 16B
	XMFLOAT4 distortionNoiseScaleSpeed;    // 16B
	XMFLOAT4 emissiveDissolveScaleSpeed;   // 16B

	XMFLOAT4 distortionMaskST;             // 16B, xy=scale zw=offset
	XMFLOAT4 opacityMaskST;                // 16B
};

}  // namespace PiercingMeshShader

// PiercingSlashMeshShader
namespace PiercingSlashMeshShader {

using PerInstanceData = BlendCGMeshShader::PerInstanceData;  // 112B
using PerFrameData    = SmokeBlendCGShader::PerFrameData;    // 80B

struct PerDrawcallData {
	BindlessIndex idxSlashTex;             // 16B
	BindlessIndex idxSlashNoiseTex;        // 16B
	BindlessIndex idxEmissiveSlashTex;     // 16B
	BindlessIndex idxEmissiveDissolveTex;  // 16B
	BindlessIndex idxDistortionNoiseTex;   // 16B
	BindlessIndex idxColorNoiseTex;        // 16B
	BindlessIndex idxMaskTex;              // 16B
	BindlessIndex idxCutoutTex;            // 16B

	u32t firstInstanceOffset;              // 4B
	u32t hasMaskTex;                       // 4B
	u32t hasCutoutTex;                     // 4B
	u32t pad0;                             // 4B

	float time;                            // 4B
	float colorBoost;                      // 4B
	float slashScale;                      // 4B
	float slashSpeed;                      // 4B

	float emissiveSlashScale;              // 4B
	float emissiveSlashSpeed;              // 4B
	float slashNoiseIntensity;             // 4B
	float distortionIntensity;             // 4B

	float emissiveIntensity;               // 4B
	float opacityBoost;                    // 4B
	float additiveLerp;                    // 4B
	float cutoutErosion;                   // 4B

	float    cutoutErosionSmoothness;      // 4B
	float    cutoutRotation;               // 4B
	XMFLOAT2 cutoutOffset;                 // 8B

	XMFLOAT4 color1;                       // 16B
	XMFLOAT4 color2;                       // 16B
	XMFLOAT4 emissiveColor;                // 16B

	XMFLOAT4 colorNoiseScaleSpeed;         // 16B, xy=scale zw=speed
	XMFLOAT4 slashNoiseScaleSpeed;         // 16B
	XMFLOAT4 emissiveDissolveScaleSpeed;   // 16B
	XMFLOAT4 distortionNoiseScaleSpeed;    // 16B

	XMFLOAT4 maskST;                       // 16B, xy=scale zw=offset
};

}  // namespace PiercingSlashMeshShader

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
	XMFLOAT4X4 lightVP;    // 현재 cascade의 VP 행렬 (camera-relative: maps posW - camPos)
	u32t       cascadeIdx;
	XMUINT3    _pfd0;
	XMFLOAT3   camPos;     // 카메라 월드 위치 (camera-relative shadow rebase)
	float      _pfd1;
};
}	// namespace ShadowMapCSMShader

// ShadowMapCSMMaskedShader — Matches shadowMapCSMMasked.hlsl (foliage alpha-test shadow).
// PerInstanceData / PerFrameData are identical to ShadowMapCSMShader; only the per-drawcall
// data carries the bindless albedo + cutoff. firstInstanceOffset stays at offset 0.
namespace ShadowMapCSMMaskedShader {
struct PerDrawcallData {
	u32t          firstInstanceOffset;
	XMUINT3       _pad0;
	BindlessIndex idxAlbedo;     // maps to HLSL int4 (range/index/arraySlice/sampler)
	float         cAlphaCutoff;
	XMFLOAT3      _pad1;
};
}	// namespace ShadowMapCSMMaskedShader

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
	i32t bakedClipId;
	i32t bakedClipFrame;
	i32t padding;
};

struct PerDrawcallData {
	u32t firstInstanceOffset;
	XMUINT3 padding;
};

struct PerFrameData {
	XMFLOAT4X4 lightVP;    // 현재 cascade의 VP 행렬 (camera-relative: maps posW - camPos)
	u32t       cascadeIdx;
	XMUINT3    _pfd0;
	XMFLOAT3   camPos;     // 카메라 월드 위치 (camera-relative shadow rebase)
	float      _pfd1;
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
    // IBL (forward parity) — must match cbuffer PerFrameData in terrain.hlsl
    XMFLOAT3      camPos;
    float         _padCam;
    BindlessIndex idxIrradiance;
    BindlessIndex idxPrefiltered;
    BindlessIndex idxBRDFLUT;
    u32t          prefilteredMipCount;
    float         iblIntensity;
    XMFLOAT2      _iblPad;
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
    XMFLOAT4X4 lightVP;    // 현재 cascade의 VP 행렬 (camera-relative: maps posW - camPos)
    u32t       cascadeIdx;
    XMUINT3    _pfd0;
    XMFLOAT3   camPos;     // 카메라 월드 위치 (camera-relative shadow rebase)
    float      _pfd1;
};
}  // namespace TerrainShadowMapCSMShader

// SwordSlashShader
namespace SwordSlashShader {

// t0 — per-instance data in StructuredBuffer
// world: row-major — CPU에서 mu::transpose().getXmf() 후 전달
struct PerInstanceData {        // 112B
    XMFLOAT4X4 world;           // 64B
    XMFLOAT4   tint;            // 16B
    XMFLOAT2   custom1;         // 8B   (Unity Custom1.xy)
    XMFLOAT2   custom2;         // 8B   (Unity Custom2.xy)
    float      t;               // 4B   (normalized particle age [0, 1])
    float      customDataEnabled;// 4B
    XMFLOAT2   pad;             // 8B
};

// b0 — bindless indices + instance offset + FX parameters (all per-drawcall)
struct PerDrawcallData {        // 240B
    BindlessIndex idxMainTex;       // 16B
    BindlessIndex idxEmissionTex;   // 16B
    BindlessIndex idxDissolveTex;   // 16B
    BindlessIndex idxFlowTex;       // 16B
    u32t          firstInstanceOffset; // 4B
    XMUINT3       pad0;             // 12B
    XMFLOAT2      speedMainTexUV;   // 8B
    XMFLOAT2      speedDissolveUV;  // 8B
    XMFLOAT2      speedFlow;        // 8B
    XMFLOAT2      padUV0;           // 8B
    XMFLOAT4      mainTexST;        // 16B
    XMFLOAT4      emissionTexST;    // 16B
    XMFLOAT4      dissolveTexST;    // 16B
    XMFLOAT4      flowTexST;        // 16B
    XMFLOAT4      addColor;         // 16B
    float         emission;         // 4B
    float         desaturation;     // 4B
    float         opacity;          // 4B
    float         flowPower;        // 4B
    float         useSmoothDissolve;// 4B  (0=hard, 1=smooth)
    float         time;             // 4B
    XMFLOAT2      remapMinMax;      // 8B  (x=min, y=max)
    XMFLOAT4      pad1;             // 16B
};

// b1 — per-frame
struct PerFrameData {           // 64B
    XMFLOAT4X4 matViewProj;     // 64B (row-major)
};

}  // namespace SwordSlashShader
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
	float cAlphaCutoff;   // foliage alpha-test threshold (0 = opaque, no discard)
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
	// Unused by the GBuffer geometry pass, but mirrors pbrDeferred.hlsl's camPos so the
	// shared pbrLighting.hlsli (camera-relative shadow space) compiles. Keep byte-aligned.
	XMFLOAT3   camPos;
	float      _padCam;
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
	i32t bakedClipId;
	i32t bakedClipFrame;
	i32t padding;
	// Energy-orb absorption ripples (body-surface emissive wave). Local effect:
	// only the absorbing player has rippleCount>0; all others default to 0.
	// Layout must match pbrDeferredSkinned.hlsl PerInstanceData (tight-packed
	// StructuredBuffer; float4 arrays avoid alignment ambiguity).
	XMFLOAT4 ripplePosAge[4];          // xyz = contact world pos, w = age (sec)
	XMFLOAT4 rippleColorIntensity[4];  // rgb = HDR color, w = intensity
	u32t     rippleCount;
	XMUINT3  ripplePad;
};

struct PerDrawcallData {
	Material material;
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
	// Unused by the GBuffer geometry pass, but mirrors pbrDeferredSkinned.hlsl's camPos so
	// the shared pbrLighting.hlsli (camera-relative shadow space) compiles. Keep byte-aligned.
	XMFLOAT3   camPos;
	float      _padCam;
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
	BindlessIndex idxGB4;   // linear view-space Z (posV.z), R32_FLOAT
	// Debug
	u32t       debugMode;
	XMUINT3    _pad0;
	// Exponential Height Fog
	BindlessIndex idxSkybox;
	float fogDensity;
    float heightFalloff;
    float fogBaseHeight;
    float _pad1;
	XMFLOAT3 camPos;
	float _pad2;
	// IBL
	BindlessIndex idxIrradiance;
	BindlessIndex idxPrefiltered;
	BindlessIndex idxBRDFLUT;
	u32t          prefilteredMipCount;
	float         iblIntensity;
	XMFLOAT2      _iblPad;
};

}	// namespace PBRDeferredLightingShader

// TonemapResolveShader — fullscreen triangle HDR -> LDR resolve pass.
// Samples the HDR scene-color RT (bindless) and writes the LDR backbuffer.
namespace TonemapResolveShader {

// Matches cbuffer PerDrawcallData : register(b0) in tonemapResolve.hlsl.
// 64 bytes.
struct PerDrawcallData {
	BindlessIndex idxSceneColor;        // HDR scene-color SRV
	BindlessIndex idxBloom;             // bloom mip0 SRV (invalid => additive bloom is a no-op)
	float         exposure;             // linear exposure multiplier applied before tonemapping
	float         bloomIntensity;       // additive bloom strength (0 => off)
	u32t          debugMode;            // GBuffer debug mode; !=0 => passthrough (skip tonemap)
	float         _pad;
	BindlessIndex idxColorGradingLUT;   // 3D LUT SRV (idxRange<0 => grading is a no-op)
};

}	// namespace TonemapResolveShader

// BloomShader — pixel-based HDR bloom (prefilter / downsample / upsample). bloom.hlsl.
namespace BloomShader {

// Matches cbuffer PerDrawcallData : register(b0) in bloom.hlsl (32 bytes).
struct PerDrawcallData {
	BindlessIndex idxSrc;        // source SRV (scene color for prefilter, a bloom mip otherwise)
	XMFLOAT2      srcTexelSize;  // 1 / source dimensions
	float         threshold;     // prefilter brightness threshold
	float         _pad;
};

}	// namespace BloomShader

// TwoSidesShader
// Port of Unity Shader Graphs/HS_Blend_TwoSides.
// Mesh-mode particles with two-sided rendering, mask + noise distortion.
namespace TwoSidesShader {

// t0 — per-instance data in StructuredBuffer
struct PerInstanceData {        // 112B  (layout matches SwordSlashShader)
    XMFLOAT4X4 world;           // 64B
    XMFLOAT4   tint;            // 16B
    XMFLOAT2   custom1;         // 8B
    XMFLOAT2   custom2;         // 8B
    float      t;               // 4B
    float      customDataEnabled;// 4B
    XMFLOAT2   pad;             // 8B
};

// b0 — bindless indices + instance offset + FX parameters (all per-drawcall)
struct PerDrawcallData {        // 256B
    BindlessIndex idxMainTex;        // 16B
    BindlessIndex idxMaskTex;        // 16B
    BindlessIndex idxNoiseTex;       // 16B
    u32t          hasNoiseTex;        // 4B
    u32t          hasMaskTex;         // 4B
    XMUINT2       pad0;               // 8B
    u32t          firstInstanceOffset; // 4B
    XMUINT3       pad1;              // 12B
    XMFLOAT4      mainTexST;         // 16B
    XMFLOAT4      maskTexST;         // 16B
    XMFLOAT4      noiseTexST;        // 16B
    XMFLOAT4      texSpeed;          // 16B  xy=main UV speed, zw=noise UV speed
    float         emission;          // 4B
    float         opacity;           // 4B
    float         useFresnel;        // 4B
    float         fresnelPower;      // 4B
    XMFLOAT4      frontFacesColor;   // 16B
    XMFLOAT4      backFacesColor;    // 16B
    XMFLOAT4      fresnelColor;      // 16B
    float         fresnelEmission;   // 4B
    float         useBackFresnel;    // 4B
    float         backFresnel;       // 4B
    float         backFresnelEmission; // 4B
    XMFLOAT4      backFresnelColor;  // 16B
    float         time;              // 4B
    XMFLOAT3      pad3;              // 12B
};

// b1 — per-frame
struct PerFrameData {           // 80B
    XMFLOAT4X4 matViewProj;     // 64B (row-major)
    XMFLOAT3   cameraPos;       // 12B
    float      cbpad;           // 4B
};

}  // namespace TwoSidesShader

// TrailShader
// Camera-facing ribbon strip rendered from a per-particle ring buffer.
// VertexCount per drawcall = (trailCount - 1) * 6 ; VS expands segments via SV_VertexID.
namespace TrailShader {

// t0 — packed trail vertices for ALL particles' trails in one StructuredBuffer.
// pos: world or emitter-local position
// age: seconds since this vertex was emitted
// cumulativeDist: arc length from the first vertex of this trail (Tile UV mode)
struct PerInstanceData {           // 32B (matches HLSL TrailVertex)
    XMFLOAT3 pos;                  // 12B
    float    age;                  // 4B
    float    cumulativeDist;       // 4B
    XMFLOAT3 pad;                  // 12B
};

// b0 — per-drawcall: one drawcall = one particle's trail.
struct PerDrawcallData {           // 144B
    XMFLOAT4X4    localToWorld;        // 64B
    BindlessIndex idxMainTex;          // 16B
    XMFLOAT4      baseColor;           // 16B

    u32t          trailStart;          // 4B   first vertex index in PerInstanceData buffer
    u32t          trailCount;          // 4B   number of vertices for this trail
    u32t          textureMode;         // 4B   0 = Stretch, 1 = Tile
    u32t          inheritParticleColor;// 4B   reserved (0/1)

    float         widthStart;          // 4B
    float         widthEnd;            // 4B
    float         widthMultiplier;     // 4B
    float         tileLength;          // 4B

    float         trailLifetime;       // 4B
    float         currentSystemTime;   // 4B
    XMFLOAT2      pad0;                // 8B
};

// b1 — per-frame.
struct PerFrameData {              // 80B
    XMFLOAT4X4 matViewProj;        // 64B (row-major)
    XMFLOAT3   cameraPos;          // 12B
    float      pad;                // 4B
};

}  // namespace TrailShader

// hiZOccluderShader
namespace HiZOccluderShader {

struct PerInstanceData {
	XMFLOAT4X4 wvp;
};

struct PerDrawcallData {
	u32t firstInstanceOffset;
	XMUINT3 padding;
};

}	// namespace HiZOccluderShader

// HiZCullShader
namespace HiZCullShader {

struct PerInstanceData {
	XMFLOAT4X4 world;
	XMFLOAT3 aabbMin;
	float padding0;
	XMFLOAT3 aabbMax;
	float padding1;
	u32t instanceGroupId;
	u32t instanceObjId;
	XMUINT2 padding2;
};

struct PerFrameData {
	XMFLOAT4X4 viewProj;
	XMFLOAT2 screenSize;
	XMFLOAT2 padding0;
	u32t objCnt;
	XMUINT3 padding1;
};

}	// namespace HiZCullShader

// HiZCompactShader
namespace HiZCompactShader {

struct PerInstanceData {
	u32t instanceGroupId;
	u32t idxCnt;
	XMUINT2 padding;
};

struct PerGroupData {
	u32t instCnt;
	u32t idxCnt;
	u32t groupOffset;
	u32t padding;
};

struct PerFrameData {
	u32t objCnt;
	XMUINT3 padding;
};

}	// namespace HiZCompactShader

// HiZClearShader
namespace HiZClearShader {

struct PerFrameData {
	u32t groupCnt;
	XMUINT3 padding;
};

}	// namespace HiZClearShader

// HiZCommandShader
namespace HiZCommandShader {

struct PerGroupData {
	u32t instCnt;
	u32t idxCnt;
	u32t groupOffset;
	u32t padding;
};

struct PerFrameData {
	u32t groupCnt;
	XMUINT3 padding;
};

struct IndirectCommand
{
	u32t groupOffset;
	DrawIndexedInstancedArgs drawArgs;
};

}	// namespace HiZCommandShader

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
		// uv' = uv * uvScaleBias.xy + uvScaleBias.zw  (identity = {1,1,0,0}).
		// Used for 9-slice: each slice samples a sub-rect of the texture.
		XMFLOAT4 uvScaleBias = { 1.f, 1.f, 0.f, 0.f };
	};

	struct PerDrawcallData {
		Material material;
		u32t firstInstanceIdx;
	};

	struct PerFrameData {
		float screenWidth;
		float screenHeight;
		float time;        // seconds; drives animated UI effects (skill charge fill)
		float padding;
	};

} // namespace UIShader

#endif	// __shader_HPP
