struct PerInstanceData {
    float4x4 world;
    float4x4 wvp;
    float4x4 wv;
    float3x3 wvNormal;
    uint rootBoneOffset;
    uint3 padding;
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
    float padding0;
    float3 cEmmisive;
    float padding1;
};

struct VSOutput {
    float4 pos : SV_Position;
    float3 posV : POSITION_V;
    float4 posL : POSITION_L;
    float3 normalV : NORMAL_V;
    float3 tangentV : TANGENT_V;
    float3 bitangentV : BITANGENT_V;
    float2 uv : UV;
};

cbuffer PerDrawcallData : register(b0) {
    Material material;
    uint firstInstanceOffset;
};

cbuffer PerFrameData : register(b1) {
    float3 globalAmbient;
    float padding0;
    uint lightCnt;
    uint3 padding1;
    int4 idxShadowMap;
    float4x4 lightVP;
}

StructuredBuffer<PerInstanceData> gInstances : register(t0);
StructuredBuffer<float4x4> gBoneData: register(t2);

static float4x4 gmtxTexturize = {
	0.5f, 0.0f, 0.0f, 0.0f,
	0.0f, -0.5f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.5f, 0.5f, 0.0f, 1.0f
};

float4x4 blendBoneTransform(uint rootBoneOffset, uint4 boneIndices, float4 boneWeights) {
    return 
        gBoneData[rootBoneOffset + boneIndices.x] * boneWeights.x +
        gBoneData[rootBoneOffset + boneIndices.y] * boneWeights.y +
        gBoneData[rootBoneOffset + boneIndices.z] * boneWeights.z +
        gBoneData[rootBoneOffset + boneIndices.w] * boneWeights.w;
}

#include "pbrLighting.hlsli"

VSOutput VSMain(
    float3 position : POSITION,
    float3 normal : NORMAL,
    float3 tangent : TANGENT,
    float3 bitangent : BITANGENT,
    float2 uv : UV,
    int4 boneIndices: BONE_INDICES,
    float4 boneWeights: BONE_WEIGHTS,
    uint idxInst : SV_InstanceID
) {
    VSOutput ret;
    
    float4x4 anim = blendBoneTransform(
        gInstances[idxInst + firstInstanceOffset].rootBoneOffset,
        boneIndices, boneWeights
    );
    
    float4 animatedPos = mul(float4(position, 1.0f), anim);
    float4 animatedNormal = mul(float4(normal, 0.0f), anim);
    
    ret.pos = mul(animatedPos, gInstances[idxInst + firstInstanceOffset].wvp);
    ret.posV = mul(animatedPos, gInstances[idxInst + firstInstanceOffset].wv).xyz;
    ret.posL = mul(
        mul(
            mul(animatedPos, gInstances[idxInst + firstInstanceOffset].world),
            lightVP
        ),
        gmtxTexturize
    );
    ret.normalV = mul(animatedNormal.xyz, gInstances[idxInst + firstInstanceOffset].wvNormal);
    if (material.idxNormal.x >= 0) {
        float4 animatedTangent = mul(float4(tangent, 0.0f), anim);
        float4 animatedBitangent = mul(float4(bitangent, 0.0f), anim);
		ret.tangentV = mul(animatedTangent.xyz, gInstances[idxInst + firstInstanceOffset].wvNormal);
		ret.bitangentV = mul(animatedBitangent.xyz, gInstances[idxInst + firstInstanceOffset].wvNormal);
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
    
    return illuminate(input.posV, input.posL, input.normalV, input.uv);
}