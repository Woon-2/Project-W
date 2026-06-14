#define MAX_CSM_CASCADES 4

struct PerInstanceData {
    float4x4 world;
    float4x4 wvp;
    float4x4 wv;
    float3x3 wvNormal;
    float3x3 worldNormal;
};

struct Material {
    int4 idxAlbedo;
    int4 idxMetallicSmoothness;
    int4 idxNormal;
    int4 idxEmmisive;
    int4 idxAmbientOcclusion;
    
    float4 cAlbedo;
    float cRoughness;
    float cMetallic;
    float cAOStrength;
    float cAlphaCutoff;   // foliage alpha-test threshold (0 = opaque)
    float3 cEmmisive;
    float padding1;
};

struct VSOutput {
    float4 pos : SV_Position;
    float3 posV : POSITION_V;
    float3 posW : POSITION_W;
    float3 normalV : NORMAL_V;
    float3 normalW : NORMAL_W;
    float3 tangentV : TANGENT_V;
    float3 bitangentV : BITANGENT_V;
    float2 uv : UV;
};

cbuffer PerDrawcallData : register(b0) {
    Material material;
    uint firstInstanceOffset;
};

cbuffer PerFrameData : register(b1) {
    float3   globalAmbient;
    float    padding0;
    uint     lightCnt;
    uint     cascadeCount;
    uint2    padding1;
    int4     idxShadowMap[MAX_CSM_CASCADES];
    float4   cascadeSplitsFarV;
    float4x4 lightVP[MAX_CSM_CASCADES];
    float4   cascadeNormalOffsets;
    // IBL (forward parity)
    float3   camPos;
    float    _padCam;
    int4     idxIrradiance;
    int4     idxPrefiltered;
    int4     idxBRDFLUT;
    uint     prefilteredMipCount;
    float    iblIntensity;
    float2   _iblPad;
}

StructuredBuffer<PerInstanceData> gInstances : register(t0);

#define IBL_ENABLED

static float4x4 gmtxTexturize = {
	0.5f, 0.0f, 0.0f, 0.0f,
	0.0f, -0.5f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.5f, 0.5f, 0.0f, 1.0f
};

#include "pbrLighting.hlsli"

VSOutput VSMain(
    float3 position : POSITION,
    float3 normal : NORMAL,
    float3 tangent : TANGENT,
    float3 bitangent : BITANGENT,
    float2 uv : UV,
    uint idxInst : SV_InstanceID
) {
    VSOutput ret;
    
    ret.pos     = mul(float4(position, 1.0f), gInstances[idxInst + firstInstanceOffset].wvp);
    ret.posV    = mul(float4(position, 1.0f), gInstances[idxInst + firstInstanceOffset].wv).xyz;
    ret.posW    = mul(float4(position, 1.0f), gInstances[idxInst + firstInstanceOffset].world).xyz;
    ret.normalV = mul(normal, gInstances[idxInst + firstInstanceOffset].wvNormal);
    ret.normalW = mul(normal, gInstances[idxInst + firstInstanceOffset].worldNormal);
    if (material.idxNormal.x >= 0) {
		ret.tangentV = mul(tangent, gInstances[idxInst + firstInstanceOffset].wvNormal);
		ret.bitangentV = mul(bitangent, gInstances[idxInst + firstInstanceOffset].wvNormal);
	}
    ret.uv = uv;
    
    return ret;
}

float4 PSMain(VSOutput input) : SV_TARGET {
    input.normalV = normalize(input.normalV);

	if (material.idxNormal.x >= 0) {
		input.tangentV = normalize(input.tangentV);
		input.bitangentV = normalize(input.bitangentV);

		float3 normal = sampleBindless(material.idxNormal, input.uv).rgb;
		normal = normal * 2.0f - 1.0f;
		
		float3x3 TBN = float3x3(input.tangentV, input.bitangentV, input.normalV);
		input.normalV = mul(normal, TBN);
	}

    // Foliage alpha test. Gated on cAlphaCutoff > 0 so opaque materials pay nothing.
    if (material.cAlphaCutoff > 0.0f) {
        float alpha = (material.idxAlbedo.x >= 0)
            ? sampleBindless(material.idxAlbedo, input.uv).a
            : material.cAlbedo.a;
        clip(alpha - material.cAlphaCutoff);
    }

    return illuminateCSM(input.posV, input.posW, input.normalV, input.uv, input.normalW);
}